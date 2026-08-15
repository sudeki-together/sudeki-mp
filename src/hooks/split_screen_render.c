#include "hooks/split_screen_render.h"

#include "engine/log.h"
#include "hooks/call_hook.h"

#include <math.h>
#include <stdint.h>
#include <string.h>

#if !defined(__GNUC__) || !defined(__i386__)
#error "Split-screen render prototype requires 32-bit GCC assembly support"
#endif

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

typedef struct SudekiMpBackdropVertex {
    float x;
    float y;
    float z;
    float reciprocal_w;
    float u;
    float v;
} SudekiMpBackdropVertex;

typedef HRESULT (__stdcall *D3DCreateRenderTargetFunction)(
    void *device,
    UINT width,
    UINT height,
    int format,
    int multisample_type,
    DWORD multisample_quality,
    BOOL lockable,
    void **surface,
    HANDLE *shared_handle
);
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
typedef HRESULT (__stdcall *D3DStretchRectFunction)(
    void *device,
    void *source_surface,
    const RECT *source_rectangle,
    void *destination_surface,
    const RECT *destination_rectangle,
    int texture_filter
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
typedef ULONG (__stdcall *ComReleaseFunction)(void *object);
typedef HRESULT (__stdcall *D3DTextureGetSurfaceLevelFunction)(
    void *texture,
    UINT level,
    void **surface
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
typedef void (*RenderStartFunction)(void);
typedef void (*FrameEndFunction)(void);
typedef void (*QuitScreenRenderFunction)(void);
typedef void (*HudPartyPointerCopyFunction)(void);
#if defined(__GNUC__) && defined(__i386__)
#define SUDEKIMP_THISCALL __attribute__((thiscall))
#endif
typedef unsigned int (SUDEKIMP_THISCALL *CharacterResourceTypeFunction)(
    void *character_resource
);
typedef BOOL (SUDEKIMP_THISCALL *CameraManagerAddCameraFunction)(
    void *manager,
    const char *name,
    const char *configuration
);
typedef void (SUDEKIMP_THISCALL *CameraManagerRemoveCameraFunction)(
    void *manager,
    const char *name
);
typedef void *(SUDEKIMP_THISCALL *CameraManagerGetCameraFunction)(
    void *manager,
    const char *name
);
enum {
    RVA_D3D_DEVICE_GLOBAL = 0x003c31dcu,
    RVA_SCENE_MANAGER_GLOBAL = 0x00408d58u,
    RVA_PC_QUIT_SCREEN_GLOBAL = 0x00408d68u,
    RVA_GAMEPLAY_HUD_GLOBAL = 0x003c2f9cu,
    RVA_ACTIVE_GROUP_GLOBAL = 0x00408d94u,
    RVA_CHARACTER_CONTROLLER_GLOBAL = 0x00408da4u,
    RVA_GAME_CAMERA_MODE_GLOBAL = 0x00408da8u,
    RVA_CAMERA_MANAGER_GLOBAL = 0x00409d7cu,
    RVA_CAMERA_MANAGER_ADD_CAMERA = 0x00036c10u,
    RVA_CAMERA_MANAGER_REMOVE_CAMERA = 0x00036de0u,
    RVA_CAMERA_MANAGER_GET_CAMERA = 0x00036ed0u,
    RVA_PC_QUIT_SCREEN_SHOW = 0x0001dbe0u,
    RVA_PC_QUIT_SCREEN_RENDER = 0x0001d690u,
    RVA_PC_QUIT_SCREEN_RENDER_CALL = 0x0028d572u,
    RVA_HUD_PARTY_POINTER_COPY = 0x000015b0u,
    RVA_HUD_GROUP_VALUES_POINTER_CALL = 0x00181517u,
    RVA_HUD_GIZMO_PORTRAIT_POINTER_CALL = 0x000aab3au,
    RVA_HUD_GIZMO_VALUES_POINTER_CALL = 0x000a9d5bu,
    RVA_HUD_GIZMO_NAME_POINTER_CALL = 0x000a9e15u,
    RVA_HUD_GIZMO_STATUS_POINTER_CALL = 0x000aacabu,
    RVA_CHARACTER_TYPE_TO_PORTRAIT_ENUM = 0x0003f430u,
    RVA_HUD_PORTRAIT_RESOURCE_SELECT = 0x0015c070u,
    RVA_HUD_PORTRAIT_RESOURCE_INDEX_TABLE = 0x002c2a94u,
    RVA_HUD_PORTRAIT_GIZMO_VTABLE = 0x002cb590u,
    RVA_CHARACTER_TYPE_TO_PORTRAIT_LOOKUP = 0x0003f498u,
    RVA_UI_RESOURCE_TABLE_INITIALIZED = 0x003c2fefu,
    RVA_RENDER_START = 0x001dce30u,
    RVA_RENDER_START_CALL = 0x0028d443u,
    RVA_FRAME_END = 0x001dd540u,
    RVA_FRAME_END_CALL = 0x0028d58cu,
    PARTY_SLOT_COUNT = 4u,
    PARTY_SLOT_ZERO_OFFSET = 0x90u,
    PARTY_SLOT_STRIDE = 0x0cu,
    CONTROLLER_TARGET_OFFSET = 0x248u,
    GAME_CAMERA_POINTER_OFFSET = 0x0cu,
    PC_QUIT_SCREEN_VISIBLE_OFFSET = 0x1c2u,
    GAMEPLAY_HUD_PORTRAIT_GIZMO_ARRAY_OFFSET = 0x138u,
    HUD_PORTRAIT_ENUM_OFFSET = 0x2a8u,
    HUD_PORTRAIT_CYCLE_ICON_OFFSET = 0x2cu,
    HUD_PORTRAIT_PARTY_INDEX_OFFSET = 0x32cu,
    CHARACTER_RESOURCE_OBJECT_OFFSET = 0x2cu,
    D3D_DEVICE_CREATE_TEXTURE_INDEX = 23u,
    D3D_DEVICE_CREATE_RENDER_TARGET_INDEX = 28u,
    D3D_DEVICE_STRETCH_RECT_INDEX = 34u,
    D3D_DEVICE_GET_RENDER_TARGET_INDEX = 38u,
    D3D_DEVICE_CREATE_STATE_BLOCK_INDEX = 59u,
    D3D_DEVICE_SET_RENDER_STATE_INDEX = 57u,
    D3D_DEVICE_SET_TEXTURE_INDEX = 65u,
    D3D_DEVICE_SET_TEXTURE_STAGE_STATE_INDEX = 67u,
    D3D_DEVICE_SET_SAMPLER_STATE_INDEX = 69u,
    D3D_DEVICE_DRAW_PRIMITIVE_UP_INDEX = 83u,
    D3D_DEVICE_SET_FVF_INDEX = 89u,
    D3D_DEVICE_SET_VERTEX_SHADER_INDEX = 92u,
    D3D_DEVICE_SET_PIXEL_SHADER_INDEX = 107u,
    D3D_SURFACE_GET_DESC_INDEX = 12u,
    D3D_TEXTURE_GET_SURFACE_LEVEL_INDEX = 18u,
    D3D_STATE_BLOCK_APPLY_INDEX = 5u,
    D3D_USAGE_RENDER_TARGET = 0x00000001u,
    D3D_POOL_DEFAULT = 0,
    D3D_STATE_BLOCK_ALL = 1,
    D3D_PRIMITIVE_TRIANGLE_STRIP = 5,
    D3D_FVF_XYZRHW_TEX1 = 0x00000104u,
    D3D_RENDER_STATE_Z_ENABLE = 7,
    D3D_RENDER_STATE_Z_WRITE_ENABLE = 14,
    D3D_RENDER_STATE_ALPHA_TEST_ENABLE = 15,
    D3D_RENDER_STATE_CULL_MODE = 22,
    D3D_RENDER_STATE_ALPHA_BLEND_ENABLE = 27,
    D3D_RENDER_STATE_FOG_ENABLE = 28,
    D3D_RENDER_STATE_LIGHTING = 137,
    D3D_RENDER_STATE_COLOR_WRITE_ENABLE = 168,
    D3D_RENDER_STATE_SCISSOR_TEST_ENABLE = 174,
    D3D_CULL_NONE = 1,
    D3D_TEXTURE_STAGE_COLOR_OPERATION = 1,
    D3D_TEXTURE_STAGE_COLOR_ARGUMENT_ONE = 2,
    D3D_TEXTURE_STAGE_ALPHA_OPERATION = 4,
    D3D_TEXTURE_STAGE_ALPHA_ARGUMENT_ONE = 5,
    D3D_TEXTURE_OPERATION_DISABLE = 1,
    D3D_TEXTURE_OPERATION_SELECT_ARGUMENT_ONE = 2,
    D3D_TEXTURE_ARGUMENT_TEXTURE = 2,
    D3D_SAMPLER_ADDRESS_U = 1,
    D3D_SAMPLER_ADDRESS_V = 2,
    D3D_SAMPLER_MAG_FILTER = 5,
    D3D_SAMPLER_MIN_FILTER = 6,
    D3D_TEXTURE_ADDRESS_CLAMP = 3,
    D3D_TEXTURE_FILTER_LINEAR_VALUE = 2,
    D3D_TEXTURE_FILTER_NONE = 0,
    D3D_TEXTURE_FILTER_LINEAR = 2,
    D3D_MULTISAMPLE_NONE = 0
};

static SudekiMpRelativeCallHook render_start_hook;
static SudekiMpRelativeCallHook frame_end_hook;
static SudekiMpRelativeCallHook quit_screen_render_hook;
static SudekiMpRelativeCallHook hud_group_values_pointer_hook;
static SudekiMpRelativeCallHook hud_gizmo_values_pointer_hook;
static SudekiMpRelativeCallHook hud_gizmo_name_pointer_hook;
static SudekiMpRelativeCallHook hud_gizmo_status_pointer_hook;
static uint8_t *game_base;
static RenderStartFunction original_render_start;
static FrameEndFunction original_frame_end;
static QuitScreenRenderFunction original_quit_screen_render
    __attribute__((used));
static HudPartyPointerCopyFunction original_hud_party_pointer_copy
    __attribute__((used));
static void *original_character_type_to_portrait_enum __attribute__((used));
static void *original_hud_portrait_resource_select __attribute__((used));
static void **d3d_device_global;
static void *composite_surface;
static void *composite_device;
static SudekiMpD3DSurfaceDesc composite_description;
static void *player_one_frame_surface;
static void *player_two_frame_surface;
static void *player_one_frame_texture;
static void *player_two_frame_texture;
static void *frame_cache_device;
static SudekiMpD3DSurfaceDesc frame_cache_description;
static BOOL player_one_frame_valid;
static BOOL player_two_frame_valid;
static BOOL compositor_logged;
static BOOL dual_compositor_logged;
static BOOL failure_logged;
static BOOL quit_backdrop_logged;
static BOOL quit_backdrop_failure_logged;
static BOOL player_two_hud_ownership_logged;
static BOOL viewport_portrait_ownership_logged;
static BOOL party_order_rotation_logged;
static int gameplay_gate_last_state = -1;
static int shared_menu_gate_last_state = -1;
static BOOL second_player_camera_enabled;
static BOOL dual_camera_frame_cache_enabled;
static UINT second_player_camera_virtual_key;
static BOOL second_player_camera_key_was_down;
static CameraManagerAddCameraFunction camera_manager_add_camera;
static CameraManagerRemoveCameraFunction camera_manager_remove_camera;
static CameraManagerGetCameraFunction camera_manager_get_camera;
static void *second_player_camera_manager;
static void *player_one_camera;
static void *player_two_camera;
static void *player_one_render_state;
static void *player_two_render_state;
static void *player_one_character;
static void *player_two_character;
static unsigned int player_two_party_slot;
static BOOL player_two_view_requested;
static BOOL rendered_player_two_this_frame;
static BOOL viewport_hud_binding_active;
static BOOL render_only_swap_active;
static void **render_only_camera_slot;
static unsigned int second_player_camera_last_rejection;

static const char second_player_camera_name[] = "SudekiMP_P2";
static const uint8_t expected_camera_manager_add_camera_entry[] = {
    0x83, 0xec, 0x14, 0x53, 0x55, 0x8b, 0x6c, 0x24,
    0x20, 0x56, 0x57, 0x8b
};
static const uint8_t expected_camera_manager_remove_camera_entry[] = {
    0x53, 0x55, 0x8b, 0x6c, 0x24, 0x0c, 0x56, 0x8b,
    0xd9, 0x57, 0x33, 0xf6
};
static const uint8_t expected_camera_manager_get_camera_entry[] = {
    0x53, 0x8b, 0x5c, 0x24, 0x08, 0x55, 0x8b, 0xe9,
    0x85, 0xdb, 0x74, 0x40
};
static const uint8_t expected_character_type_to_portrait_enum_entry[] = {
    0x48, 0x83, 0xf8, 0x22, 0x77, 0x38, 0x0f, 0xb6, 0x80
};
static const uint8_t expected_hud_portrait_resource_select_entry[] = {
    0x56, 0x57, 0x50, 0x83, 0xec, 0x0c, 0x80, 0x3d
};
static const uint8_t expected_hud_portrait_resource_index_table[] = {
    0x15, 0x01, 0x00, 0x00, 0x16, 0x01, 0x00, 0x00,
    0x17, 0x01, 0x00, 0x00, 0x18, 0x01, 0x00, 0x00
};
static const uint8_t expected_pc_quit_screen_show_entry[] = {
    0x56, 0x8b, 0x35, 0x68, 0x8d, 0x80, 0x00, 0x85,
    0xf6, 0x74, 0x0a, 0x8b
};

static BOOL pc_quit_screen_show_signature_matches(uint8_t *base) {
    uint32_t relocated_singleton_address;

    if (base == NULL || memcmp(
            base + RVA_PC_QUIT_SCREEN_SHOW,
            expected_pc_quit_screen_show_entry,
            3u) != 0 || memcmp(
            base + RVA_PC_QUIT_SCREEN_SHOW + 7u,
            expected_pc_quit_screen_show_entry + 7u,
            sizeof(expected_pc_quit_screen_show_entry) - 7u) != 0) {
        return FALSE;
    }
    memcpy(
        &relocated_singleton_address,
        base + RVA_PC_QUIT_SCREEN_SHOW + 3u,
        sizeof(relocated_singleton_address)
    );
    return relocated_singleton_address ==
        (uint32_t)(uintptr_t)(base + RVA_PC_QUIT_SCREEN_GLOBAL);
}

static BOOL portrait_selector_signatures_match(uint8_t *base) {
    uint32_t relocated_lookup_address;
    uint32_t relocated_resource_initialized_address;

    if (base == NULL || memcmp(
            base + RVA_CHARACTER_TYPE_TO_PORTRAIT_ENUM,
            expected_character_type_to_portrait_enum_entry,
            sizeof(expected_character_type_to_portrait_enum_entry)) != 0 ||
        memcmp(
            base + RVA_HUD_PORTRAIT_RESOURCE_SELECT,
            expected_hud_portrait_resource_select_entry,
            sizeof(expected_hud_portrait_resource_select_entry)) != 0 ||
        *(base + RVA_HUD_PORTRAIT_RESOURCE_SELECT + 12u) != 0u ||
        memcmp(
            base + RVA_HUD_PORTRAIT_RESOURCE_INDEX_TABLE,
            expected_hud_portrait_resource_index_table,
            sizeof(expected_hud_portrait_resource_index_table)) != 0) {
        return FALSE;
    }
    memcpy(
        &relocated_lookup_address,
        base + RVA_CHARACTER_TYPE_TO_PORTRAIT_ENUM + 9u,
        sizeof(relocated_lookup_address)
    );
    memcpy(
        &relocated_resource_initialized_address,
        base + RVA_HUD_PORTRAIT_RESOURCE_SELECT + 8u,
        sizeof(relocated_resource_initialized_address)
    );
    return relocated_lookup_address == (uint32_t)(uintptr_t)(
            base + RVA_CHARACTER_TYPE_TO_PORTRAIT_LOOKUP
        ) &&
        relocated_resource_initialized_address == (uint32_t)(uintptr_t)(
            base + RVA_UI_RESOURCE_TABLE_INITIALIZED
        );
}

static BOOL readable_memory(const void *pointer, size_t size) {
    MEMORY_BASIC_INFORMATION information;
    uintptr_t start;
    uintptr_t end;
    uintptr_t region_end;

    if (pointer == NULL || size == 0u ||
        VirtualQuery(pointer, &information, sizeof(information)) == 0 ||
        information.State != MEM_COMMIT ||
        (information.Protect & (PAGE_NOACCESS | PAGE_GUARD)) != 0) {
        return FALSE;
    }
    start = (uintptr_t)pointer;
    end = start + size;
    region_end = (uintptr_t)information.BaseAddress +
        information.RegionSize;
    return end >= start && end <= region_end;
}

static BOOL process_owns_foreground(void) {
    HWND foreground = GetForegroundWindow();
    DWORD process_id = 0;

    if (foreground != NULL) {
        GetWindowThreadProcessId(foreground, &process_id);
    }
    return process_id == GetCurrentProcessId();
}

static void log_second_player_camera_rejection_once(
    unsigned int rejection,
    const char *reason
) {
    if (second_player_camera_last_rejection == rejection) {
        return;
    }
    second_player_camera_last_rejection = rejection;
    SudekiMpLogFormat(
        "split_screen_render event=second_player_camera phase=reject reason=%s\r\n",
        reason
    );
}

static BOOL character_position(void *character, float output[3]) {
    uint8_t *transform;

    if (!readable_memory(character, 0x48u)) {
        return FALSE;
    }
    transform = *(uint8_t **)((uint8_t *)character + 0x44u);
    if (!readable_memory(transform, 0x24u)) {
        return FALSE;
    }
    output[0] = *(float *)(transform + 0x18u);
    output[1] = *(float *)(transform + 0x1cu);
    output[2] = *(float *)(transform + 0x20u);
    return isfinite(output[0]) && isfinite(output[1]) &&
        isfinite(output[2]);
}

static void *current_camera_manager(void) {
    void *manager;

    if (game_base == NULL || !readable_memory(
            game_base + RVA_CAMERA_MANAGER_GLOBAL,
            sizeof(manager))) {
        return NULL;
    }
    manager = *(void **)(game_base + RVA_CAMERA_MANAGER_GLOBAL);
    return readable_memory(manager, 0x4cu) ? manager : NULL;
}

static void *current_render_camera(void *manager) {
    return readable_memory(manager, 0x24u) ?
        *(void **)((uint8_t *)manager + 0x20u) : NULL;
}

static BOOL pc_quit_screen_visible(void) {
    uint8_t *quit_screen;

    if (game_base == NULL || !readable_memory(
            game_base + RVA_PC_QUIT_SCREEN_GLOBAL,
            sizeof(quit_screen))) {
        return FALSE;
    }
    quit_screen = *(uint8_t **)(game_base + RVA_PC_QUIT_SCREEN_GLOBAL);
    return readable_memory(
            quit_screen,
            PC_QUIT_SCREEN_VISIBLE_OFFSET + 1u) &&
        quit_screen[PC_QUIT_SCREEN_VISIBLE_OFFSET] != 0u;
}

void *SudekiMpSplitScreenHudPartySourceDispatch(void *source) {
    uint8_t *group;
    uint8_t *player_one_source;
    uint8_t *player_two_source;
    void *desired_character;
    unsigned int index;

    if (!viewport_hud_binding_active || !dual_camera_frame_cache_enabled ||
        game_base == NULL ||
        player_two_party_slot == 0u ||
        player_two_party_slot >= PARTY_SLOT_COUNT ||
        player_one_character == NULL || player_two_character == NULL ||
        !readable_memory(
            game_base + RVA_ACTIVE_GROUP_GLOBAL,
            sizeof(group))) {
        return source;
    }
    group = *(uint8_t **)(game_base + RVA_ACTIVE_GROUP_GLOBAL);
    if (!readable_memory(
            group + PARTY_SLOT_ZERO_OFFSET,
            PARTY_SLOT_COUNT * PARTY_SLOT_STRIDE)) {
        return source;
    }
    player_one_source = group + PARTY_SLOT_ZERO_OFFSET;
    player_two_source = player_one_source +
        player_two_party_slot * PARTY_SLOT_STRIDE;
    if (source == player_one_source) {
        desired_character = rendered_player_two_this_frame ?
            player_two_character : player_one_character;
        if (!player_two_hud_ownership_logged) {
            player_two_hud_ownership_logged = TRUE;
            SudekiMpLogFormat(
                "split_screen_render event=player_two_hud_ownership phase=active player_one_character=0x%08lx player_two_character=0x%08lx player_two_party_slot=%u policy=stable_character_identity_per_viewport\r\n",
                (unsigned long)(uintptr_t)player_one_character,
                (unsigned long)(uintptr_t)player_two_character,
                player_two_party_slot
            );
        }
    } else if (source == player_two_source) {
        desired_character = rendered_player_two_this_frame ?
            player_one_character : player_two_character;
    } else {
        return source;
    }
    for (index = 0u; index < PARTY_SLOT_COUNT; ++index) {
        uint8_t *candidate_source = player_one_source +
            index * PARTY_SLOT_STRIDE;
        if (*(void **)candidate_source == desired_character) {
            return candidate_source;
        }
    }
    return source;
}

__attribute__((naked, noinline, used))
static void split_screen_hud_party_pointer_copy_entry(void) {
    __asm__ volatile(
        "pushl %eax\n\t"
        "pushl %ecx\n\t"
        "call _SudekiMpSplitScreenHudPartySourceDispatch\n\t"
        "addl $4, %esp\n\t"
        "movl %eax, %ecx\n\t"
        "popl %eax\n\t"
        "call *_original_hud_party_pointer_copy\n\t"
        "ret\n\t"
    );
}

__attribute__((naked, noinline, used))
static unsigned int map_character_type_to_portrait_enum(
    unsigned int character_type
) {
    (void)character_type;
    __asm__ volatile(
        "movl 4(%esp), %eax\n\t"
        "call *_original_character_type_to_portrait_enum\n\t"
        "ret\n\t"
    );
}

/*
 * FUN_0055C070 uses Sudeki's internal register convention: ECX is the
 * resource-table index, EAX is the synchronous-load flag, and its one stack
 * argument is the UIElementCycleIcon receiver. The native helper consumes
 * that stack argument itself.
 */
__attribute__((naked, noinline, used))
static void assign_portrait_resource_synchronously(
    void *cycle_icon,
    unsigned int resource_index
) {
    (void)cycle_icon;
    (void)resource_index;
    __asm__ volatile(
        "movl 8(%esp), %ecx\n\t"
        "pushl 4(%esp)\n\t"
        "movl $1, %eax\n\t"
        "call *_original_hud_portrait_resource_select\n\t"
        "ret\n\t"
    );
}

static void refresh_viewport_portraits(void) {
    uint8_t *hud;
    uint8_t *group;
    uint8_t *party_source;
    uint8_t *character;
    void *character_resource;
    void **character_resource_vtable;
    CharacterResourceTypeFunction get_character_resource_type;
    void *portrait_gizmo;
    void *expected_vtable;
    unsigned int group_size;
    unsigned int index;
    unsigned int source_index;
    unsigned int character_type;
    unsigned int portrait_enum;
    unsigned int resource_index;
    unsigned int refreshed_count = 0u;

    if (!dual_camera_frame_cache_enabled || game_base == NULL ||
        original_character_type_to_portrait_enum == NULL ||
        original_hud_portrait_resource_select == NULL ||
        !readable_memory(
            game_base + RVA_GAMEPLAY_HUD_GLOBAL,
            sizeof(hud)) ||
        !readable_memory(
            game_base + RVA_ACTIVE_GROUP_GLOBAL,
            sizeof(group))) {
        return;
    }
    hud = *(uint8_t **)(game_base + RVA_GAMEPLAY_HUD_GLOBAL);
    group = *(uint8_t **)(game_base + RVA_ACTIVE_GROUP_GLOBAL);
    if (!readable_memory(
            hud,
            GAMEPLAY_HUD_PORTRAIT_GIZMO_ARRAY_OFFSET +
                PARTY_SLOT_COUNT * sizeof(portrait_gizmo)) ||
        !readable_memory(group, 0xd0u) ||
        !readable_memory(
            group + PARTY_SLOT_ZERO_OFFSET,
            PARTY_SLOT_COUNT * PARTY_SLOT_STRIDE)) {
        return;
    }
    group_size = *(unsigned int *)(group + 0xccu);
    if (group_size > PARTY_SLOT_COUNT) {
        group_size = PARTY_SLOT_COUNT;
    }
    expected_vtable = game_base + RVA_HUD_PORTRAIT_GIZMO_VTABLE;
    for (index = 0u; index < group_size; ++index) {
        portrait_gizmo = *(void **)(
            hud + GAMEPLAY_HUD_PORTRAIT_GIZMO_ARRAY_OFFSET +
                index * sizeof(portrait_gizmo)
        );
        if (!readable_memory(portrait_gizmo, 0x330u) ||
            *(void **)portrait_gizmo != expected_vtable ||
            *(unsigned int *)(
                (uint8_t *)portrait_gizmo +
                HUD_PORTRAIT_PARTY_INDEX_OFFSET
            ) != index) {
            continue;
        }
        source_index = index;
        party_source = group + PARTY_SLOT_ZERO_OFFSET +
            source_index * PARTY_SLOT_STRIDE;
        character = *(uint8_t **)party_source;
        if (index == 0u) {
            character = (uint8_t *)(rendered_player_two_this_frame ?
                player_two_character : player_one_character);
        } else if (index == player_two_party_slot) {
            character = (uint8_t *)(rendered_player_two_this_frame ?
                player_one_character : player_two_character);
        }
        if (!readable_memory(
                character,
                CHARACTER_RESOURCE_OBJECT_OFFSET + sizeof(void *))) {
            continue;
        }
        character_resource = character + CHARACTER_RESOURCE_OBJECT_OFFSET;
        character_resource_vtable = *(void ***)character_resource;
        if (!readable_memory(
                character_resource_vtable,
                5u * sizeof(void *))) {
            continue;
        }
        get_character_resource_type = (CharacterResourceTypeFunction)(
            character_resource_vtable[4]
        );
        if (!readable_memory(
                (const void *)get_character_resource_type,
                sizeof(void *))) {
            continue;
        }
        character_type = get_character_resource_type(character_resource);
        portrait_enum = map_character_type_to_portrait_enum(character_type);
        if ((*(uint8_t *)((uint8_t *)portrait_gizmo + 0x2acu) & 1u) != 0u) {
            portrait_enum += 8u;
        }
        if (portrait_enum >= 16u || !readable_memory(
                game_base + RVA_HUD_PORTRAIT_RESOURCE_INDEX_TABLE +
                    portrait_enum * sizeof(resource_index),
                sizeof(resource_index))) {
            continue;
        }
        *(unsigned int *)(
            (uint8_t *)portrait_gizmo + HUD_PORTRAIT_ENUM_OFFSET
        ) = portrait_enum;
        resource_index = *(unsigned int *)(
            game_base + RVA_HUD_PORTRAIT_RESOURCE_INDEX_TABLE +
                portrait_enum * sizeof(resource_index)
        );
        assign_portrait_resource_synchronously(
            (uint8_t *)portrait_gizmo + HUD_PORTRAIT_CYCLE_ICON_OFFSET,
            resource_index
        );
        ++refreshed_count;
    }
    if (!viewport_portrait_ownership_logged &&
        rendered_player_two_this_frame && refreshed_count != 0u) {
        viewport_portrait_ownership_logged = TRUE;
        SudekiMpLogFormat(
            "split_screen_render event=player_two_hud_portrait phase=active refreshed_gizmos=%u player_two_party_slot=%u policy=direct_synchronous_cycle_icon_resource_assignment\r\n",
            refreshed_count,
            player_two_party_slot
        );
    }
}

static BOOL resolve_player_characters(
    void **first_character,
    void **second_character,
    unsigned int *second_slot
) {
    uint8_t *group;
    uint8_t *controller;
    void *controller_target;
    unsigned int index;

    if (game_base == NULL || !readable_memory(
            game_base + RVA_ACTIVE_GROUP_GLOBAL,
            sizeof(group)) ||
        !readable_memory(
            game_base + RVA_CHARACTER_CONTROLLER_GLOBAL,
            sizeof(controller))) {
        return FALSE;
    }
    group = *(uint8_t **)(game_base + RVA_ACTIVE_GROUP_GLOBAL);
    controller = *(uint8_t **)(
        game_base + RVA_CHARACTER_CONTROLLER_GLOBAL
    );
    if (group == NULL || controller == NULL ||
        !readable_memory(
            group + PARTY_SLOT_ZERO_OFFSET,
            PARTY_SLOT_COUNT * PARTY_SLOT_STRIDE) ||
        !readable_memory(
            controller + CONTROLLER_TARGET_OFFSET,
            sizeof(controller_target))) {
        return FALSE;
    }
    controller_target = *(void **)(
        controller + CONTROLLER_TARGET_OFFSET
    );
    if (controller_target == NULL ||
        *(void **)(group + PARTY_SLOT_ZERO_OFFSET) != controller_target) {
        return FALSE;
    }
    for (index = 1u; index < PARTY_SLOT_COUNT; ++index) {
        void *candidate = *(void **)(
            group + PARTY_SLOT_ZERO_OFFSET + index * PARTY_SLOT_STRIDE
        );
        if (candidate != NULL && candidate != controller_target) {
            *first_character = controller_target;
            *second_character = candidate;
            *second_slot = index;
            return TRUE;
        }
    }
    return FALSE;
}

static void **current_scene_render_camera_slot(void) {
    uint8_t *scene_manager;
    uint8_t *renderer;

    if (game_base == NULL || !readable_memory(
            game_base + RVA_SCENE_MANAGER_GLOBAL,
            sizeof(scene_manager))) {
        return NULL;
    }
    scene_manager = *(uint8_t **)(game_base + RVA_SCENE_MANAGER_GLOBAL);
    if (!readable_memory(scene_manager, 0x44u)) {
        return NULL;
    }
    renderer = *(uint8_t **)(scene_manager + 0x40u);
    return readable_memory(renderer, 0x80u) ?
        (void **)(renderer + 0x7cu) : NULL;
}

static BOOL update_player_two_render_state(void) {
    float first_position[3];
    float second_position[3];
    float matrix[16];
    uint16_t *generation;

    if (!readable_memory(player_one_render_state, 0xdcu) ||
        !readable_memory(player_two_render_state, 0xdcu) ||
        !character_position(player_one_character, first_position) ||
        !character_position(player_two_character, second_position)) {
        return FALSE;
    }
    memcpy(
        matrix,
        (uint8_t *)player_one_render_state + 0x90u,
        sizeof(matrix)
    );
    matrix[12] += second_position[0] - first_position[0];
    matrix[13] += second_position[1] - first_position[1];
    matrix[14] += second_position[2] - first_position[2];
    matrix[15] = 1.0f;
    if (!isfinite(matrix[12]) || !isfinite(matrix[13]) ||
        !isfinite(matrix[14])) {
        return FALSE;
    }
    memcpy(
        (uint8_t *)player_two_render_state + 0x90u,
        matrix,
        sizeof(matrix)
    );
    memcpy(
        (uint8_t *)player_two_render_state + 0xd0u,
        (uint8_t *)player_one_render_state + 0xd0u,
        sizeof(float) * 3u
    );
    generation = (uint16_t *)((uint8_t *)player_two_render_state + 0x2cu);
    ++*generation;
    return TRUE;
}

static BOOL restore_render_only_camera(void) {
    void **slot;

    if (!render_only_swap_active) {
        return TRUE;
    }
    slot = render_only_camera_slot;
    render_only_swap_active = FALSE;
    render_only_camera_slot = NULL;
    if (!readable_memory(slot, sizeof(*slot))) {
        log_second_player_camera_rejection_once(
            12u,
            "scene_render_slot_unavailable_during_restore"
        );
        return FALSE;
    }
    if (*slot == player_two_render_state) {
        *slot = player_one_render_state;
        return TRUE;
    }
    if (*slot != player_one_render_state) {
        log_second_player_camera_rejection_once(
            13u,
            "scene_render_owner_changed_during_frame"
        );
        return FALSE;
    }
    return TRUE;
}

static void apply_render_only_camera(void) {
    void **slot;

    if (!second_player_camera_enabled || !player_two_view_requested ||
        player_two_camera == NULL) {
        return;
    }
    if (current_render_camera(second_player_camera_manager) !=
        player_one_camera) {
        log_second_player_camera_rejection_once(
            14u,
            "global_render_camera_not_player_one"
        );
        player_two_view_requested = FALSE;
        return;
    }
    slot = current_scene_render_camera_slot();
    if (slot == NULL || *slot != player_one_render_state) {
        log_second_player_camera_rejection_once(
            15u,
            "scene_render_owner_not_player_one"
        );
        player_two_view_requested = FALSE;
        return;
    }
    if (!update_player_two_render_state()) {
        log_second_player_camera_rejection_once(
            16u,
            "translated_render_state_update_failed"
        );
        player_two_view_requested = FALSE;
        return;
    }
    *slot = player_two_render_state;
    render_only_camera_slot = slot;
    render_only_swap_active = TRUE;
    second_player_camera_last_rejection = 0u;
}

static void invalidate_dual_frame_cache(void) {
    player_one_frame_valid = FALSE;
    player_two_frame_valid = FALSE;
    dual_compositor_logged = FALSE;
}

static BOOL release_player_two_camera(const char *reason) {
    void *manager = second_player_camera_manager;
    void *current;

    if (player_two_camera == NULL) {
        player_two_view_requested = FALSE;
        rendered_player_two_this_frame = FALSE;
        viewport_hud_binding_active = FALSE;
        invalidate_dual_frame_cache();
        return TRUE;
    }
    restore_render_only_camera();
    if (!readable_memory(manager, 0x4cu)) {
        log_second_player_camera_rejection_once(
            8u,
            "camera_manager_unavailable_during_release"
        );
        return FALSE;
    }
    current = current_render_camera(manager);
    if (current == player_two_camera) {
        log_second_player_camera_rejection_once(
            9u,
            "refusing_to_remove_globally_selected_player_two_camera"
        );
        return FALSE;
    }
    camera_manager_remove_camera(manager, second_player_camera_name);
    SudekiMpLogFormat(
        "split_screen_render event=second_player_camera phase=release reason=%s\r\n",
        reason == NULL ? "unspecified" : reason
    );
    second_player_camera_manager = NULL;
    player_one_camera = NULL;
    player_two_camera = NULL;
    player_one_render_state = NULL;
    player_two_render_state = NULL;
    player_one_character = NULL;
    player_two_character = NULL;
    player_two_party_slot = 0u;
    player_two_view_requested = FALSE;
    rendered_player_two_this_frame = FALSE;
    viewport_hud_binding_active = FALSE;
    invalidate_dual_frame_cache();
    return TRUE;
}

static BOOL acquire_player_two_camera(void) {
    void *manager;
    void *first_character;
    void *second_character;
    void **scene_camera_slot;
    unsigned int second_slot;

    manager = current_camera_manager();
    if (manager == NULL) {
        log_second_player_camera_rejection_once(1u, "camera_manager_unavailable");
        return FALSE;
    }
    if (!resolve_player_characters(
            &first_character,
            &second_character,
            &second_slot)) {
        log_second_player_camera_rejection_once(2u, "second_party_character_unavailable");
        return FALSE;
    }
    player_one_camera = current_render_camera(manager);
    player_one_character = first_character;
    player_two_character = second_character;
    player_two_party_slot = second_slot;
    if (!readable_memory(player_one_camera, 0x108u)) {
        log_second_player_camera_rejection_once(3u, "player_one_camera_unavailable");
        player_one_camera = NULL;
        player_one_character = NULL;
        player_two_character = NULL;
        return FALSE;
    }
    player_one_render_state = *(void **)(
        (uint8_t *)player_one_camera + 0x34u
    );
    if (camera_manager_get_camera(manager, second_player_camera_name) != NULL ||
        !camera_manager_add_camera(
            manager,
            second_player_camera_name,
            "default")) {
        log_second_player_camera_rejection_once(4u, "add_named_camera_failed");
        player_one_camera = NULL;
        player_one_character = NULL;
        player_two_character = NULL;
        return FALSE;
    }
    player_two_camera = camera_manager_get_camera(
        manager,
        second_player_camera_name
    );
    second_player_camera_manager = manager;
    if (!readable_memory(player_two_camera, 0x108u)) {
        log_second_player_camera_rejection_once(5u, "named_camera_unavailable");
        release_player_two_camera("acquire_failed");
        return FALSE;
    }
    player_two_render_state = *(void **)(
        (uint8_t *)player_two_camera + 0x34u
    );
    scene_camera_slot = current_scene_render_camera_slot();
    if (!readable_memory(player_one_render_state, 0xdcu) ||
        !readable_memory(player_two_render_state, 0xdcu) ||
        scene_camera_slot == NULL || *scene_camera_slot != player_one_render_state) {
        log_second_player_camera_rejection_once(6u, "render_state_ownership_invalid");
        release_player_two_camera("acquire_failed");
        return FALSE;
    }
    if (!update_player_two_render_state()) {
        log_second_player_camera_rejection_once(7u, "initial_render_state_update_failed");
        release_player_two_camera("acquire_failed");
        return FALSE;
    }
    second_player_camera_last_rejection = 0u;
    SudekiMpLogFormat(
        "split_screen_render event=second_player_camera phase=acquire manager=0x%08lx player_one_camera=0x%08lx player_two_camera=0x%08lx player_one_render_state=0x%08lx player_two_render_state=0x%08lx player_one_character=0x%08lx player_two_character=0x%08lx player_two_party_slot=%u toggle_virtual_key=0x%02lx policy=%s\r\n",
        (unsigned long)(uintptr_t)manager,
        (unsigned long)(uintptr_t)player_one_camera,
        (unsigned long)(uintptr_t)player_two_camera,
        (unsigned long)(uintptr_t)player_one_render_state,
        (unsigned long)(uintptr_t)player_two_render_state,
        (unsigned long)(uintptr_t)player_one_character,
        (unsigned long)(uintptr_t)player_two_character,
        player_two_party_slot,
        (unsigned long)second_player_camera_virtual_key,
        dual_camera_frame_cache_enabled ?
            "alternating_render_state_frame_cache" :
            "translated_native_render_state_no_global_camera_switch"
    );
    return TRUE;
}

static void poll_second_player_camera(BOOL gameplay_allowed) {
    BOOL key_is_down;
    void *first_character;
    void *second_character;
    unsigned int second_slot;
    void *current;

    if (!second_player_camera_enabled) {
        return;
    }
    key_is_down =
        (GetAsyncKeyState((int)second_player_camera_virtual_key) & 0x8000) != 0;
    if (!gameplay_allowed) {
        release_player_two_camera("gameplay_gate_inactive");
        second_player_camera_key_was_down = key_is_down;
        return;
    }
    if (player_two_camera == NULL && !acquire_player_two_camera()) {
        second_player_camera_key_was_down = key_is_down;
        return;
    }
    if (!resolve_player_characters(
            &first_character,
            &second_character,
            &second_slot)) {
        release_player_two_camera("party_assignment_changed");
        second_player_camera_key_was_down = key_is_down;
        return;
    }
    if (first_character != player_one_character ||
        second_character != player_two_character ||
        second_slot != player_two_party_slot) {
        if (first_character == player_two_character &&
            second_character == player_one_character &&
            second_slot == player_two_party_slot) {
            if (!party_order_rotation_logged) {
                party_order_rotation_logged = TRUE;
                SudekiMpLogFormat(
                    "split_screen_render event=party_order_rotation phase=tolerated player_one_character=0x%08lx player_two_character=0x%08lx player_two_party_slot=%u policy=keep_stable_multiplayer_camera_identity\r\n",
                    (unsigned long)(uintptr_t)player_one_character,
                    (unsigned long)(uintptr_t)player_two_character,
                    player_two_party_slot
                );
            }
        } else {
            release_player_two_camera("party_assignment_changed");
            second_player_camera_key_was_down = key_is_down;
            return;
        }
    }
    current = current_render_camera(second_player_camera_manager);
    if (current != player_one_camera) {
        release_player_two_camera("native_render_camera_changed");
        second_player_camera_key_was_down = key_is_down;
        return;
    }
    if (dual_camera_frame_cache_enabled) {
        player_two_view_requested = !rendered_player_two_this_frame;
        second_player_camera_key_was_down = key_is_down;
        return;
    }
    if (process_owns_foreground() && key_is_down &&
        !second_player_camera_key_was_down) {
        player_two_view_requested = !player_two_view_requested;
        second_player_camera_last_rejection = 0u;
        SudekiMpLogFormat(
            "split_screen_render event=second_player_camera phase=switch active_player=%u global_render_camera=0x%08lx scene_render_state=0x%08lx focus_character=0x%08lx policy=render_only_swap_restore_before_end_scene\r\n",
            player_two_view_requested ? 2u : 1u,
            (unsigned long)(uintptr_t)player_one_camera,
            (unsigned long)(uintptr_t)(player_two_view_requested ?
                player_two_render_state : player_one_render_state),
            (unsigned long)(uintptr_t)(player_two_view_requested ?
                player_two_character : player_one_character)
        );
    }
    second_player_camera_key_was_down = key_is_down;
}

static void log_failure_once(const char *reason, HRESULT result) {
    if (failure_logged) {
        return;
    }
    failure_logged = TRUE;
    SudekiMpLogFormat(
        "split_screen_render event=compositor_failure reason=%s result=0x%08lx fallback=unchanged_native_frame\r\n",
        reason,
        (unsigned long)result
    );
}

static BOOL gameplay_split_allowed(const char **reason) {
    uint8_t *group;
    uint8_t *controller;
    uint8_t *camera_mode;
    void *front_character;
    void *controller_target;
    void *camera_pointer;

    if (game_base == NULL) {
        *reason = "game_base_unavailable";
        return FALSE;
    }
    group = *(uint8_t **)(game_base + RVA_ACTIVE_GROUP_GLOBAL);
    controller = *(uint8_t **)(
        game_base + RVA_CHARACTER_CONTROLLER_GLOBAL
    );
    camera_mode = *(uint8_t **)(
        game_base + RVA_GAME_CAMERA_MODE_GLOBAL
    );
    if (group == NULL) {
        *reason = "active_group_unavailable";
        return FALSE;
    }
    if (controller == NULL) {
        *reason = "character_controller_unavailable";
        return FALSE;
    }
    if (camera_mode == NULL) {
        *reason = "game_camera_mode_unavailable";
        return FALSE;
    }
    if (!readable_memory(
            group + PARTY_SLOT_ZERO_OFFSET,
            sizeof(front_character))) {
        *reason = "active_group_unavailable";
        return FALSE;
    }
    if (!readable_memory(
            controller + CONTROLLER_TARGET_OFFSET,
            sizeof(controller_target))) {
        *reason = "character_controller_unavailable";
        return FALSE;
    }
    if (!readable_memory(
            camera_mode + GAME_CAMERA_POINTER_OFFSET,
            sizeof(camera_pointer))) {
        *reason = "game_camera_mode_unavailable";
        return FALSE;
    }
    front_character = *(void **)(group + PARTY_SLOT_ZERO_OFFSET);
    controller_target = *(void **)(
        controller + CONTROLLER_TARGET_OFFSET
    );
    camera_pointer = *(void **)(
        camera_mode + GAME_CAMERA_POINTER_OFFSET
    );
    if (front_character == NULL || controller_target == NULL ||
        front_character != controller_target) {
        *reason = "front_character_not_controller_owned";
        return FALSE;
    }
    if (!readable_memory(camera_pointer, sizeof(void *))) {
        *reason = "game_camera_unavailable";
        return FALSE;
    }
    *reason = "active_party_controller_camera";
    return TRUE;
}

static void log_gameplay_gate(BOOL allowed, const char *reason) {
    int state = allowed ? 1 : 0;

    if (state == gameplay_gate_last_state) {
        return;
    }
    gameplay_gate_last_state = state;
    SudekiMpLogFormat(
        "split_screen_render event=gameplay_gate state=%s reason=%s\r\n",
        allowed ? "active" : "inactive",
        reason
    );
}

static void log_shared_menu_gate(BOOL visible) {
    int state = visible ? 1 : 0;

    if (state == shared_menu_gate_last_state) {
        return;
    }
    shared_menu_gate_last_state = state;
    SudekiMpLogFormat(
        "split_screen_render event=shared_menu_gate state=%s reason=pc_quit_screen_visible policy=native_full_width_ui_over_frozen_cached_camera_pair\r\n",
        visible ? "active" : "inactive"
    );
}

static void release_com_object(void **object) {
    void **vtable;
    ComReleaseFunction release;

    if (object == NULL || !readable_memory(*object, sizeof(void *))) {
        if (object != NULL) {
            *object = NULL;
        }
        return;
    }
    vtable = *(void ***)*object;
    if (!readable_memory(vtable, 3u * sizeof(void *))) {
        *object = NULL;
        return;
    }
    release = (ComReleaseFunction)vtable[2];
    release(*object);
    *object = NULL;
}

static void release_dual_frame_surfaces(void) {
    release_com_object(&player_one_frame_surface);
    release_com_object(&player_two_frame_surface);
    release_com_object(&player_one_frame_texture);
    release_com_object(&player_two_frame_texture);
    frame_cache_device = NULL;
    ZeroMemory(&frame_cache_description, sizeof(frame_cache_description));
    invalidate_dual_frame_cache();
}

static BOOL same_surface_description(
    const SudekiMpD3DSurfaceDesc *left,
    const SudekiMpD3DSurfaceDesc *right
) {
    return left->width == right->width && left->height == right->height &&
        left->format == right->format;
}

static BOOL ensure_composite_surface(
    void *device,
    D3DCreateRenderTargetFunction create_render_target,
    const SudekiMpD3DSurfaceDesc *source_description
) {
    HRESULT result;

    if (composite_surface != NULL && composite_device == device &&
        same_surface_description(
            &composite_description,
            source_description)) {
        return TRUE;
    }
    release_com_object(&composite_surface);
    composite_device = NULL;
    ZeroMemory(&composite_description, sizeof(composite_description));
    result = create_render_target(
        device,
        source_description->width,
        source_description->height,
        source_description->format,
        D3D_MULTISAMPLE_NONE,
        0u,
        FALSE,
        &composite_surface,
        NULL
    );
    if (FAILED(result) || composite_surface == NULL) {
        log_failure_once("create_composite_surface_failed", result);
        composite_surface = NULL;
        return FALSE;
    }
    composite_device = device;
    composite_description = *source_description;
    composite_description.multisample_type = D3D_MULTISAMPLE_NONE;
    composite_description.multisample_quality = 0u;
    return TRUE;
}

static BOOL ensure_dual_frame_surfaces(
    void *device,
    D3DCreateTextureFunction create_texture,
    const SudekiMpD3DSurfaceDesc *source_description
) {
    void **texture_vtable;
    D3DTextureGetSurfaceLevelFunction get_surface_level;
    HRESULT result;

    if (player_one_frame_surface != NULL &&
        player_two_frame_surface != NULL &&
        player_one_frame_texture != NULL &&
        player_two_frame_texture != NULL &&
        frame_cache_device == device &&
        same_surface_description(
            &frame_cache_description,
            source_description)) {
        return TRUE;
    }
    release_dual_frame_surfaces();
    result = create_texture(
        device,
        source_description->width,
        source_description->height,
        1u,
        D3D_USAGE_RENDER_TARGET,
        source_description->format,
        D3D_POOL_DEFAULT,
        &player_one_frame_texture,
        NULL
    );
    if (FAILED(result) || player_one_frame_texture == NULL) {
        log_failure_once("create_player_one_frame_texture_failed", result);
        release_dual_frame_surfaces();
        return FALSE;
    }
    texture_vtable = *(void ***)player_one_frame_texture;
    if (!readable_memory(
            texture_vtable,
            (D3D_TEXTURE_GET_SURFACE_LEVEL_INDEX + 1u) * sizeof(void *))) {
        log_failure_once("player_one_texture_vtable_unavailable", E_POINTER);
        release_dual_frame_surfaces();
        return FALSE;
    }
    get_surface_level = (D3DTextureGetSurfaceLevelFunction)
        texture_vtable[D3D_TEXTURE_GET_SURFACE_LEVEL_INDEX];
    result = get_surface_level(
        player_one_frame_texture,
        0u,
        &player_one_frame_surface
    );
    if (FAILED(result) || player_one_frame_surface == NULL) {
        log_failure_once("get_player_one_frame_surface_failed", result);
        release_dual_frame_surfaces();
        return FALSE;
    }
    result = create_texture(
        device,
        source_description->width,
        source_description->height,
        1u,
        D3D_USAGE_RENDER_TARGET,
        source_description->format,
        D3D_POOL_DEFAULT,
        &player_two_frame_texture,
        NULL
    );
    if (FAILED(result) || player_two_frame_texture == NULL) {
        log_failure_once("create_player_two_frame_texture_failed", result);
        release_dual_frame_surfaces();
        return FALSE;
    }
    texture_vtable = *(void ***)player_two_frame_texture;
    if (!readable_memory(
            texture_vtable,
            (D3D_TEXTURE_GET_SURFACE_LEVEL_INDEX + 1u) * sizeof(void *))) {
        log_failure_once("player_two_texture_vtable_unavailable", E_POINTER);
        release_dual_frame_surfaces();
        return FALSE;
    }
    get_surface_level = (D3DTextureGetSurfaceLevelFunction)
        texture_vtable[D3D_TEXTURE_GET_SURFACE_LEVEL_INDEX];
    result = get_surface_level(
        player_two_frame_texture,
        0u,
        &player_two_frame_surface
    );
    if (FAILED(result) || player_two_frame_surface == NULL) {
        log_failure_once("get_player_two_frame_surface_failed", result);
        release_dual_frame_surfaces();
        return FALSE;
    }
    frame_cache_device = device;
    frame_cache_description = *source_description;
    frame_cache_description.multisample_type = D3D_MULTISAMPLE_NONE;
    frame_cache_description.multisample_quality = 0u;
    return TRUE;
}

static void compose_native_frame(void) {
    void *device;
    void **device_vtable;
    D3DCreateRenderTargetFunction create_render_target;
    D3DStretchRectFunction stretch_rect;
    D3DGetRenderTargetFunction get_render_target;
    void *native_surface = NULL;
    void **surface_vtable;
    D3DSurfaceGetDescFunction get_description;
    SudekiMpD3DSurfaceDesc description;
    RECT source_rectangle;
    RECT left_rectangle;
    RECT right_rectangle;
    HRESULT result;

    if (d3d_device_global == NULL ||
        !readable_memory(d3d_device_global, sizeof(*d3d_device_global))) {
        log_failure_once("device_global_unavailable", E_POINTER);
        return;
    }
    device = *d3d_device_global;
    if (!readable_memory(device, sizeof(void *))) {
        log_failure_once("device_unavailable", E_POINTER);
        return;
    }
    device_vtable = *(void ***)device;
    if (!readable_memory(
            device_vtable,
            (D3D_DEVICE_GET_RENDER_TARGET_INDEX + 1u) * sizeof(void *))) {
        log_failure_once("device_vtable_unavailable", E_POINTER);
        return;
    }
    create_render_target = (D3DCreateRenderTargetFunction)
        device_vtable[D3D_DEVICE_CREATE_RENDER_TARGET_INDEX];
    stretch_rect = (D3DStretchRectFunction)
        device_vtable[D3D_DEVICE_STRETCH_RECT_INDEX];
    get_render_target = (D3DGetRenderTargetFunction)
        device_vtable[D3D_DEVICE_GET_RENDER_TARGET_INDEX];
    if (create_render_target == NULL || stretch_rect == NULL ||
        get_render_target == NULL) {
        log_failure_once("compositor_method_unavailable", E_POINTER);
        return;
    }
    result = get_render_target(device, 0u, &native_surface);
    if (FAILED(result) || !readable_memory(native_surface, sizeof(void *))) {
        log_failure_once("get_native_surface_failed", result);
        release_com_object(&native_surface);
        return;
    }
    surface_vtable = *(void ***)native_surface;
    if (!readable_memory(
            surface_vtable,
            (D3D_SURFACE_GET_DESC_INDEX + 1u) * sizeof(void *))) {
        log_failure_once("surface_vtable_unavailable", E_POINTER);
        release_com_object(&native_surface);
        return;
    }
    get_description = (D3DSurfaceGetDescFunction)
        surface_vtable[D3D_SURFACE_GET_DESC_INDEX];
    result = get_description(native_surface, &description);
    if (FAILED(result) || description.width < 2u || description.height == 0u) {
        log_failure_once("get_surface_description_failed", result);
        release_com_object(&native_surface);
        return;
    }
    if (description.multisample_type != D3D_MULTISAMPLE_NONE) {
        log_failure_once("multisampled_backbuffer_unsupported", E_NOTIMPL);
        release_com_object(&native_surface);
        return;
    }
    if (!ensure_composite_surface(
            device,
            create_render_target,
            &description)) {
        release_com_object(&native_surface);
        return;
    }

    source_rectangle.left = 0;
    source_rectangle.top = 0;
    source_rectangle.right = (LONG)description.width;
    source_rectangle.bottom = (LONG)description.height;
    left_rectangle = source_rectangle;
    left_rectangle.right = (LONG)(description.width / 2u);
    right_rectangle = source_rectangle;
    right_rectangle.left = left_rectangle.right;

    result = stretch_rect(
        device,
        native_surface,
        &source_rectangle,
        composite_surface,
        &source_rectangle,
        D3D_TEXTURE_FILTER_NONE
    );
    if (FAILED(result)) {
        log_failure_once("capture_native_frame_failed", result);
        release_com_object(&native_surface);
        return;
    }
    result = stretch_rect(
        device,
        composite_surface,
        &source_rectangle,
        native_surface,
        &left_rectangle,
        D3D_TEXTURE_FILTER_LINEAR
    );
    if (SUCCEEDED(result)) {
        result = stretch_rect(
            device,
            composite_surface,
            &source_rectangle,
            native_surface,
            &right_rectangle,
            D3D_TEXTURE_FILTER_LINEAR
        );
    }
    if (FAILED(result)) {
        stretch_rect(
            device,
            composite_surface,
            &source_rectangle,
            native_surface,
            &source_rectangle,
            D3D_TEXTURE_FILTER_NONE
        );
        log_failure_once("compose_split_frame_failed", result);
        release_com_object(&native_surface);
        return;
    }
    if (!compositor_logged) {
        compositor_logged = TRUE;
        SudekiMpLogFormat(
            "split_screen_render event=compositor_active source=%ux%u format=%d multisample=%d layout=left_right camera_policy=duplicate_finished_native_frame\r\n",
            description.width,
            description.height,
            description.format,
            description.multisample_type
        );
    }
    release_com_object(&native_surface);
}

static void compose_cached_camera_frames(BOOL rendered_player_two) {
    void *device;
    void **device_vtable;
    D3DCreateTextureFunction create_texture;
    D3DStretchRectFunction stretch_rect;
    D3DGetRenderTargetFunction get_render_target;
    void *native_surface = NULL;
    void *current_frame_surface;
    void **surface_vtable;
    D3DSurfaceGetDescFunction get_description;
    SudekiMpD3DSurfaceDesc description;
    RECT source_rectangle;
    RECT left_rectangle;
    RECT right_rectangle;
    HRESULT result;

    if (d3d_device_global == NULL ||
        !readable_memory(d3d_device_global, sizeof(*d3d_device_global))) {
        log_failure_once("device_global_unavailable", E_POINTER);
        return;
    }
    device = *d3d_device_global;
    if (!readable_memory(device, sizeof(void *))) {
        log_failure_once("device_unavailable", E_POINTER);
        return;
    }
    device_vtable = *(void ***)device;
    if (!readable_memory(
            device_vtable,
            (D3D_DEVICE_GET_RENDER_TARGET_INDEX + 1u) * sizeof(void *))) {
        log_failure_once("device_vtable_unavailable", E_POINTER);
        return;
    }
    create_texture = (D3DCreateTextureFunction)
        device_vtable[D3D_DEVICE_CREATE_TEXTURE_INDEX];
    stretch_rect = (D3DStretchRectFunction)
        device_vtable[D3D_DEVICE_STRETCH_RECT_INDEX];
    get_render_target = (D3DGetRenderTargetFunction)
        device_vtable[D3D_DEVICE_GET_RENDER_TARGET_INDEX];
    if (create_texture == NULL || stretch_rect == NULL ||
        get_render_target == NULL) {
        log_failure_once("compositor_method_unavailable", E_POINTER);
        return;
    }
    result = get_render_target(device, 0u, &native_surface);
    if (FAILED(result) || !readable_memory(native_surface, sizeof(void *))) {
        log_failure_once("get_native_surface_failed", result);
        release_com_object(&native_surface);
        return;
    }
    surface_vtable = *(void ***)native_surface;
    if (!readable_memory(
            surface_vtable,
            (D3D_SURFACE_GET_DESC_INDEX + 1u) * sizeof(void *))) {
        log_failure_once("surface_vtable_unavailable", E_POINTER);
        release_com_object(&native_surface);
        return;
    }
    get_description = (D3DSurfaceGetDescFunction)
        surface_vtable[D3D_SURFACE_GET_DESC_INDEX];
    result = get_description(native_surface, &description);
    if (FAILED(result) || description.width < 2u || description.height == 0u) {
        log_failure_once("get_surface_description_failed", result);
        release_com_object(&native_surface);
        return;
    }
    if (description.multisample_type != D3D_MULTISAMPLE_NONE) {
        log_failure_once("multisampled_backbuffer_unsupported", E_NOTIMPL);
        release_com_object(&native_surface);
        return;
    }
    if (!ensure_dual_frame_surfaces(
            device,
            create_texture,
            &description)) {
        release_com_object(&native_surface);
        return;
    }

    source_rectangle.left = 0;
    source_rectangle.top = 0;
    source_rectangle.right = (LONG)description.width;
    source_rectangle.bottom = (LONG)description.height;
    left_rectangle = source_rectangle;
    left_rectangle.right = (LONG)(description.width / 2u);
    right_rectangle = source_rectangle;
    right_rectangle.left = left_rectangle.right;
    current_frame_surface = rendered_player_two ?
        player_two_frame_surface : player_one_frame_surface;
    result = stretch_rect(
        device,
        native_surface,
        &source_rectangle,
        current_frame_surface,
        &source_rectangle,
        D3D_TEXTURE_FILTER_NONE
    );
    if (FAILED(result)) {
        log_failure_once("capture_camera_frame_failed", result);
        release_com_object(&native_surface);
        return;
    }
    if (rendered_player_two) {
        player_two_frame_valid = TRUE;
    } else {
        player_one_frame_valid = TRUE;
    }
    if (!player_one_frame_valid || !player_two_frame_valid) {
        release_com_object(&native_surface);
        return;
    }

    result = stretch_rect(
        device,
        player_one_frame_surface,
        &source_rectangle,
        native_surface,
        &left_rectangle,
        D3D_TEXTURE_FILTER_LINEAR
    );
    if (SUCCEEDED(result)) {
        result = stretch_rect(
            device,
            player_two_frame_surface,
            &source_rectangle,
            native_surface,
            &right_rectangle,
            D3D_TEXTURE_FILTER_LINEAR
        );
    }
    if (FAILED(result)) {
        stretch_rect(
            device,
            current_frame_surface,
            &source_rectangle,
            native_surface,
            &source_rectangle,
            D3D_TEXTURE_FILTER_NONE
        );
        log_failure_once("compose_cached_camera_frames_failed", result);
        release_com_object(&native_surface);
        return;
    }
    if (!dual_compositor_logged) {
        dual_compositor_logged = TRUE;
        SudekiMpLogFormat(
            "split_screen_render event=dual_camera_cache_active source=%ux%u format=%d multisample=%d layout=player_one_left_player_two_right cadence=alternate_engine_frames maximum_cache_age_frames=1 render_passes_per_engine_frame=1\r\n",
            description.width,
            description.height,
            description.format,
            description.multisample_type
        );
    }
    release_com_object(&native_surface);
}

static void log_quit_backdrop_failure_once(
    const char *reason,
    HRESULT result
) {
    if (quit_backdrop_failure_logged) {
        return;
    }
    quit_backdrop_failure_logged = TRUE;
    SudekiMpLogFormat(
        "split_screen_render event=quit_backdrop_failure reason=%s result=0x%08lx fallback=native_player_one_background\r\n",
        reason,
        (unsigned long)result
    );
}

static BOOL draw_cached_camera_backdrop(void) {
    void *device;
    void **device_vtable;
    void *state_block = NULL;
    void **state_block_vtable;
    D3DCreateStateBlockFunction create_state_block;
    D3DSetRenderStateFunction set_render_state;
    D3DSetTextureFunction set_texture;
    D3DSetTextureStageStateFunction set_texture_stage_state;
    D3DSetSamplerStateFunction set_sampler_state;
    D3DSetShaderFunction set_vertex_shader;
    D3DSetShaderFunction set_pixel_shader;
    D3DSetFvfFunction set_fvf;
    D3DDrawPrimitiveUpFunction draw_primitive_up;
    D3DStateBlockApplyFunction apply_state_block;
    SudekiMpBackdropVertex left_vertices[4];
    SudekiMpBackdropVertex right_vertices[4];
    float left;
    float top;
    float middle;
    float right;
    float bottom;
    HRESULT result;
    HRESULT restore_result;
    BOOL drawn = FALSE;

    if (!dual_camera_frame_cache_enabled ||
        !player_one_frame_valid || !player_two_frame_valid ||
        player_one_frame_texture == NULL ||
        player_two_frame_texture == NULL ||
        d3d_device_global == NULL ||
        !readable_memory(d3d_device_global, sizeof(*d3d_device_global))) {
        return FALSE;
    }
    device = *d3d_device_global;
    if (device == NULL || device != frame_cache_device ||
        !readable_memory(device, sizeof(void *))) {
        return FALSE;
    }
    device_vtable = *(void ***)device;
    if (!readable_memory(
            device_vtable,
            (D3D_DEVICE_SET_PIXEL_SHADER_INDEX + 1u) * sizeof(void *))) {
        log_quit_backdrop_failure_once("device_vtable_unavailable", E_POINTER);
        return FALSE;
    }
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
    if (create_state_block == NULL || set_render_state == NULL ||
        set_texture == NULL || set_texture_stage_state == NULL ||
        set_sampler_state == NULL || draw_primitive_up == NULL ||
        set_fvf == NULL || set_vertex_shader == NULL ||
        set_pixel_shader == NULL) {
        log_quit_backdrop_failure_once("device_method_unavailable", E_POINTER);
        return FALSE;
    }
    result = create_state_block(device, D3D_STATE_BLOCK_ALL, &state_block);
    if (FAILED(result) || state_block == NULL ||
        !readable_memory(state_block, sizeof(void *))) {
        log_quit_backdrop_failure_once("create_state_block_failed", result);
        release_com_object(&state_block);
        return FALSE;
    }
    state_block_vtable = *(void ***)state_block;
    if (!readable_memory(
            state_block_vtable,
            (D3D_STATE_BLOCK_APPLY_INDEX + 1u) * sizeof(void *))) {
        log_quit_backdrop_failure_once(
            "state_block_vtable_unavailable",
            E_POINTER
        );
        release_com_object(&state_block);
        return FALSE;
    }
    apply_state_block = (D3DStateBlockApplyFunction)
        state_block_vtable[D3D_STATE_BLOCK_APPLY_INDEX];
    if (apply_state_block == NULL) {
        log_quit_backdrop_failure_once(
            "state_block_apply_unavailable",
            E_POINTER
        );
        release_com_object(&state_block);
        return FALSE;
    }

    left = -0.5f;
    top = -0.5f;
    middle = (float)(frame_cache_description.width / 2u) - 0.5f;
    right = (float)frame_cache_description.width - 0.5f;
    bottom = (float)frame_cache_description.height - 0.5f;
    left_vertices[0] = (SudekiMpBackdropVertex){left, top, 0.0f, 1.0f, 0.0f, 0.0f};
    left_vertices[1] = (SudekiMpBackdropVertex){middle, top, 0.0f, 1.0f, 1.0f, 0.0f};
    left_vertices[2] = (SudekiMpBackdropVertex){left, bottom, 0.0f, 1.0f, 0.0f, 1.0f};
    left_vertices[3] = (SudekiMpBackdropVertex){middle, bottom, 0.0f, 1.0f, 1.0f, 1.0f};
    right_vertices[0] = (SudekiMpBackdropVertex){middle, top, 0.0f, 1.0f, 0.0f, 0.0f};
    right_vertices[1] = (SudekiMpBackdropVertex){right, top, 0.0f, 1.0f, 1.0f, 0.0f};
    right_vertices[2] = (SudekiMpBackdropVertex){middle, bottom, 0.0f, 1.0f, 0.0f, 1.0f};
    right_vertices[3] = (SudekiMpBackdropVertex){right, bottom, 0.0f, 1.0f, 1.0f, 1.0f};

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
    result = set_render_state(device, D3D_RENDER_STATE_ALPHA_BLEND_ENABLE, FALSE);
    if (FAILED(result)) goto restore_state;
    result = set_render_state(device, D3D_RENDER_STATE_FOG_ENABLE, FALSE);
    if (FAILED(result)) goto restore_state;
    result = set_render_state(device, D3D_RENDER_STATE_LIGHTING, FALSE);
    if (FAILED(result)) goto restore_state;
    result = set_render_state(device, D3D_RENDER_STATE_COLOR_WRITE_ENABLE, 0x0fu);
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
        device, 0u, D3D_SAMPLER_ADDRESS_U, D3D_TEXTURE_ADDRESS_CLAMP);
    if (FAILED(result)) goto restore_state;
    result = set_sampler_state(
        device, 0u, D3D_SAMPLER_ADDRESS_V, D3D_TEXTURE_ADDRESS_CLAMP);
    if (FAILED(result)) goto restore_state;
    result = set_sampler_state(
        device, 0u, D3D_SAMPLER_MAG_FILTER,
        D3D_TEXTURE_FILTER_LINEAR_VALUE);
    if (FAILED(result)) goto restore_state;
    result = set_sampler_state(
        device, 0u, D3D_SAMPLER_MIN_FILTER,
        D3D_TEXTURE_FILTER_LINEAR_VALUE);
    if (FAILED(result)) goto restore_state;
    result = set_texture(device, 0u, player_one_frame_texture);
    if (FAILED(result)) goto restore_state;
    result = draw_primitive_up(
        device,
        D3D_PRIMITIVE_TRIANGLE_STRIP,
        2u,
        left_vertices,
        sizeof(left_vertices[0])
    );
    if (FAILED(result)) goto restore_state;
    result = set_texture(device, 0u, player_two_frame_texture);
    if (FAILED(result)) goto restore_state;
    result = draw_primitive_up(
        device,
        D3D_PRIMITIVE_TRIANGLE_STRIP,
        2u,
        right_vertices,
        sizeof(right_vertices[0])
    );
    if (SUCCEEDED(result)) {
        drawn = TRUE;
    }

restore_state:
    restore_result = apply_state_block(state_block);
    release_com_object(&state_block);
    if (FAILED(result)) {
        log_quit_backdrop_failure_once("draw_cached_pair_failed", result);
        return FALSE;
    }
    if (FAILED(restore_result)) {
        log_quit_backdrop_failure_once(
            "restore_state_block_failed",
            restore_result
        );
        return FALSE;
    }
    if (drawn && !quit_backdrop_logged) {
        quit_backdrop_logged = TRUE;
        SudekiMpLogFormat(
            "split_screen_render event=quit_backdrop_active source=%ux%u layout=frozen_player_one_left_player_two_right layer=native_quit_ui_over_cached_gameplay\r\n",
            frame_cache_description.width,
            frame_cache_description.height
        );
    }
    return drawn;
}

void SudekiMpSplitScreenRenderStartDispatch(void) {
    BOOL quit_menu_visible;

    rendered_player_two_this_frame = FALSE;
    viewport_hud_binding_active = FALSE;
    original_render_start();
    quit_menu_visible = pc_quit_screen_visible();
    if (!quit_menu_visible) {
        apply_render_only_camera();
    }
    rendered_player_two_this_frame = render_only_swap_active;
    if (!quit_menu_visible) {
        refresh_viewport_portraits();
        viewport_hud_binding_active = TRUE;
    }
}

void SudekiMpSplitScreenQuitScreenRenderDispatch(void) {
    if (pc_quit_screen_visible()) {
        draw_cached_camera_backdrop();
    }
}

__attribute__((naked, noinline, used))
static void split_screen_quit_screen_render_entry(void) {
    __asm__ volatile(
        "pushfl\n\t"
        "pushal\n\t"
        "call _SudekiMpSplitScreenQuitScreenRenderDispatch\n\t"
        "popal\n\t"
        "popfl\n\t"
        "call *_original_quit_screen_render\n\t"
        "ret\n\t"
    );
}

__attribute__((naked, noinline, used))
static void split_screen_render_start_entry(void) {
    __asm__ volatile(
        "call _SudekiMpSplitScreenRenderStartDispatch\n\t"
        "ret\n\t"
    );
}

void SudekiMpSplitScreenFrameEndDispatch(void) {
    const char *gate_reason;
    BOOL split_allowed;
    BOOL quit_menu_visible;
    BOOL presentation_allowed;

    viewport_hud_binding_active = FALSE;
    restore_render_only_camera();
    original_frame_end();
    split_allowed = gameplay_split_allowed(&gate_reason);
    quit_menu_visible = pc_quit_screen_visible();
    presentation_allowed = split_allowed && !quit_menu_visible;
    log_gameplay_gate(split_allowed, gate_reason);
    log_shared_menu_gate(quit_menu_visible);
    if (presentation_allowed) {
        if (dual_camera_frame_cache_enabled) {
            compose_cached_camera_frames(rendered_player_two_this_frame);
        } else {
            compose_native_frame();
        }
    }
    if (!quit_menu_visible) {
        poll_second_player_camera(split_allowed);
    }
    rendered_player_two_this_frame = FALSE;
}

__attribute__((naked, noinline, used))
static void split_screen_frame_end_entry(void) {
    __asm__ volatile(
        "call _SudekiMpSplitScreenFrameEndDispatch\n\t"
        "ret\n\t"
    );
}

BOOL SudekiMpInstallSplitScreenRender(
    HMODULE game_module,
    BOOL enable_second_player_camera,
    BOOL enable_dual_camera_frame_cache,
    UINT toggle_second_player_camera_virtual_key
) {
    uint8_t *base;

    if (game_module == NULL || original_render_start != NULL ||
        original_frame_end != NULL ||
        (enable_dual_camera_frame_cache &&
         !enable_second_player_camera) ||
        (enable_second_player_camera &&
         (toggle_second_player_camera_virtual_key == 0u ||
          toggle_second_player_camera_virtual_key > 0xffu))) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    base = (uint8_t *)game_module;
    if (!pc_quit_screen_show_signature_matches(base)) {
        SetLastError(ERROR_INVALID_DATA);
        return FALSE;
    }
    if (enable_second_player_camera &&
        (memcmp(
            base + RVA_CAMERA_MANAGER_ADD_CAMERA,
            expected_camera_manager_add_camera_entry,
            sizeof(expected_camera_manager_add_camera_entry)) != 0 ||
         memcmp(
            base + RVA_CAMERA_MANAGER_REMOVE_CAMERA,
            expected_camera_manager_remove_camera_entry,
            sizeof(expected_camera_manager_remove_camera_entry)) != 0 ||
         memcmp(
            base + RVA_CAMERA_MANAGER_GET_CAMERA,
            expected_camera_manager_get_camera_entry,
            sizeof(expected_camera_manager_get_camera_entry)) != 0 ||
         (enable_dual_camera_frame_cache &&
          !portrait_selector_signatures_match(base)))) {
        SetLastError(ERROR_INVALID_DATA);
        return FALSE;
    }
    game_base = base;
    original_render_start = (RenderStartFunction)(
        game_base + RVA_RENDER_START
    );
    original_frame_end = (FrameEndFunction)(game_base + RVA_FRAME_END);
    original_quit_screen_render = (QuitScreenRenderFunction)(
        game_base + RVA_PC_QUIT_SCREEN_RENDER
    );
    original_hud_party_pointer_copy = (HudPartyPointerCopyFunction)(
        game_base + RVA_HUD_PARTY_POINTER_COPY
    );
    original_character_type_to_portrait_enum =
        game_base + RVA_CHARACTER_TYPE_TO_PORTRAIT_ENUM;
    original_hud_portrait_resource_select =
        game_base + RVA_HUD_PORTRAIT_RESOURCE_SELECT;
    d3d_device_global = (void **)(game_base + RVA_D3D_DEVICE_GLOBAL);
    second_player_camera_enabled = enable_second_player_camera;
    dual_camera_frame_cache_enabled = enable_dual_camera_frame_cache;
    second_player_camera_virtual_key =
        toggle_second_player_camera_virtual_key;
    second_player_camera_key_was_down = FALSE;
    camera_manager_add_camera = (CameraManagerAddCameraFunction)(
        game_base + RVA_CAMERA_MANAGER_ADD_CAMERA
    );
    camera_manager_remove_camera = (CameraManagerRemoveCameraFunction)(
        game_base + RVA_CAMERA_MANAGER_REMOVE_CAMERA
    );
    camera_manager_get_camera = (CameraManagerGetCameraFunction)(
        game_base + RVA_CAMERA_MANAGER_GET_CAMERA
    );
    player_two_view_requested = FALSE;
    rendered_player_two_this_frame = FALSE;
    viewport_hud_binding_active = FALSE;
    render_only_swap_active = FALSE;
    render_only_camera_slot = NULL;
    player_two_hud_ownership_logged = FALSE;
    viewport_portrait_ownership_logged = FALSE;
    party_order_rotation_logged = FALSE;
    if (!SudekiMpInstallRelativeCallHook(
            &render_start_hook,
            game_base + RVA_RENDER_START_CALL,
            original_render_start,
            split_screen_render_start_entry) ||
        !SudekiMpInstallRelativeCallHook(
            &frame_end_hook,
            game_base + RVA_FRAME_END_CALL,
            original_frame_end,
            split_screen_frame_end_entry) ||
        !SudekiMpInstallRelativeCallHook(
            &quit_screen_render_hook,
            game_base + RVA_PC_QUIT_SCREEN_RENDER_CALL,
            original_quit_screen_render,
            split_screen_quit_screen_render_entry) ||
        (dual_camera_frame_cache_enabled &&
         (!SudekiMpInstallRelativeCallHook(
              &hud_group_values_pointer_hook,
              game_base + RVA_HUD_GROUP_VALUES_POINTER_CALL,
              original_hud_party_pointer_copy,
              split_screen_hud_party_pointer_copy_entry) ||
          !SudekiMpInstallRelativeCallHook(
              &hud_gizmo_values_pointer_hook,
              game_base + RVA_HUD_GIZMO_VALUES_POINTER_CALL,
              original_hud_party_pointer_copy,
              split_screen_hud_party_pointer_copy_entry) ||
          !SudekiMpInstallRelativeCallHook(
              &hud_gizmo_name_pointer_hook,
              game_base + RVA_HUD_GIZMO_NAME_POINTER_CALL,
              original_hud_party_pointer_copy,
              split_screen_hud_party_pointer_copy_entry) ||
          !SudekiMpInstallRelativeCallHook(
              &hud_gizmo_status_pointer_hook,
              game_base + RVA_HUD_GIZMO_STATUS_POINTER_CALL,
              original_hud_party_pointer_copy,
              split_screen_hud_party_pointer_copy_entry)))) {
        SudekiMpUninstallSplitScreenRender();
        return FALSE;
    }
    SudekiMpLogFormat(
        "split_screen_render event=install render_start_rva=0x%08lx render_start_callsite_rva=0x%08lx frame_end_rva=0x%08lx frame_end_callsite_rva=0x%08lx quit_render_rva=0x%08lx quit_render_callsite_rva=0x%08lx scope=render_only_camera_swap_plus_post_end_scene_compositor gameplay_state_gated shared_menu_gate=pc_quit_screen_plus_0x1c2 shared_menu_backdrop=frozen_cached_camera_pair_before_native_quit_ui layout=left_right camera_policy=%s second_player_named_camera=%s dual_camera_frame_cache=%s viewport_hud_party_slot_swap=%s viewport_hud_portrait_assignment=%s toggle_virtual_key=0x%02lx\r\n",
        (unsigned long)RVA_RENDER_START,
        (unsigned long)RVA_RENDER_START_CALL,
        (unsigned long)RVA_FRAME_END,
        (unsigned long)RVA_FRAME_END_CALL,
        (unsigned long)RVA_PC_QUIT_SCREEN_RENDER,
        (unsigned long)RVA_PC_QUIT_SCREEN_RENDER_CALL,
        dual_camera_frame_cache_enabled ?
            "alternating_clean_frame_cache" :
            "duplicate_finished_native_frame",
        second_player_camera_enabled ? "true" : "false",
        dual_camera_frame_cache_enabled ? "true" : "false",
        dual_camera_frame_cache_enabled ? "true" : "false",
        dual_camera_frame_cache_enabled ?
            "direct_synchronous_cycle_icon_resource" : "disabled",
        (unsigned long)second_player_camera_virtual_key
    );
    return TRUE;
}

void SudekiMpUninstallSplitScreenRender(void) {
    restore_render_only_camera();
    SudekiMpRestoreRelativeCallHook(&hud_gizmo_status_pointer_hook);
    SudekiMpRestoreRelativeCallHook(&hud_gizmo_name_pointer_hook);
    SudekiMpRestoreRelativeCallHook(&hud_gizmo_values_pointer_hook);
    SudekiMpRestoreRelativeCallHook(&hud_group_values_pointer_hook);
    SudekiMpRestoreRelativeCallHook(&quit_screen_render_hook);
    SudekiMpRestoreRelativeCallHook(&render_start_hook);
    SudekiMpRestoreRelativeCallHook(&frame_end_hook);
    release_player_two_camera("module_uninstall");
    release_com_object(&composite_surface);
    release_dual_frame_surfaces();
    composite_device = NULL;
    ZeroMemory(&composite_description, sizeof(composite_description));
    compositor_logged = FALSE;
    dual_compositor_logged = FALSE;
    failure_logged = FALSE;
    quit_backdrop_logged = FALSE;
    quit_backdrop_failure_logged = FALSE;
    player_two_hud_ownership_logged = FALSE;
    viewport_portrait_ownership_logged = FALSE;
    party_order_rotation_logged = FALSE;
    gameplay_gate_last_state = -1;
    shared_menu_gate_last_state = -1;
    second_player_camera_enabled = FALSE;
    dual_camera_frame_cache_enabled = FALSE;
    second_player_camera_virtual_key = 0u;
    second_player_camera_key_was_down = FALSE;
    camera_manager_add_camera = NULL;
    camera_manager_remove_camera = NULL;
    camera_manager_get_camera = NULL;
    second_player_camera_manager = NULL;
    player_one_camera = NULL;
    player_two_camera = NULL;
    player_one_render_state = NULL;
    player_two_render_state = NULL;
    player_one_character = NULL;
    player_two_character = NULL;
    player_two_party_slot = 0u;
    player_two_view_requested = FALSE;
    rendered_player_two_this_frame = FALSE;
    viewport_hud_binding_active = FALSE;
    render_only_swap_active = FALSE;
    render_only_camera_slot = NULL;
    second_player_camera_last_rejection = 0u;
    d3d_device_global = NULL;
    original_render_start = NULL;
    original_frame_end = NULL;
    original_quit_screen_render = NULL;
    original_hud_party_pointer_copy = NULL;
    original_character_type_to_portrait_enum = NULL;
    original_hud_portrait_resource_select = NULL;
    game_base = NULL;
}
