#include "cleanroom/menu.h"

#include "cleanroom/audio.h"
#include "cleanroom/engine.h"
#include "engine/blacksmith_ui_presenter.h"
#include "engine/log.h"
#include "engine/player_statehood.h"
#include "engine/save_book_vote.h"
#include "engine/save_book_vote_input.h"
#include "engine/transition_vote.h"
#include "hooks/call_hook.h"
#include "hooks/blacksmith_ui_adapter.h"
#include "hooks/control_separation.h"
#include "hooks/save_book_intercept.h"
#include "hooks/split_screen_render.h"
#include "hooks/talos_coop_balance.h"
#include "hooks/zone_transition_trace.h"
#include "input/bridge_receiver.h"

#include <stdint.h>
#include <limits.h>
#include <math.h>
#include <string.h>

#if !defined(__GNUC__) || !defined(__i386__)
#error "Cleanroom menu requires the 32-bit Windows target"
#endif

typedef void (__attribute__((thiscall)) *ControllerUpdateFunction)(
    void *controller,
    void *update_data
);
typedef unsigned int (__attribute__((thiscall)) *FrontEndActionFunction)(
    void *controller,
    unsigned int phase,
    unsigned int event,
    unsigned int argument
);
typedef void (__attribute__((thiscall)) *FrontEndUpdateFunction)(
    void *controller,
    float delta_seconds
);
typedef void (__attribute__((thiscall)) *FrontEndRenderFunction)(
    void *controller
);
typedef void (__attribute__((stdcall)) *FrontEndMenuBuilderFunction)(
    void *controller
);
typedef void (__attribute__((stdcall)) *FrontEndSelectionRefreshFunction)(void *controller);
typedef void (__attribute__((stdcall)) *FrontEndStateUpdateFunction)(void *controller);
typedef void (__attribute__((thiscall)) *OptionsRowsInitializeFunction)(
    void *options_page
);
typedef void (__attribute__((thiscall)) *UiElementBarUpdateFunction)(
    void *row
);
typedef void *(__attribute__((cdecl)) *GameAllocateFunction)(
    unsigned int size
);
typedef BOOL (__attribute__((cdecl)) *MoviePlayFunction)(
    const char *movie_name,
    BOOL skippable
);
typedef BOOL (__attribute__((cdecl)) *MovieStopFunction)(void);
typedef void *(__attribute__((thiscall)) *AnimatedTitleRowDestructorFunction)(
    void *row,
    unsigned int deleting
);
typedef void *(__attribute__((cdecl)) *NativeTextureCreateFunction)(
    const int *resource_name,
    unsigned int request_flags,
    unsigned int residency_flags,
    unsigned int variant,
    unsigned int request_owner,
    unsigned int request_tag
);
typedef void *(__attribute__((thiscall)) *NativeTextureDestructorFunction)(
    void *texture,
    unsigned int deleting
);
typedef void *(__attribute__((thiscall)) *NativeTextureGpuHandleFunction)(
    void *texture
);
typedef void *(__attribute__((thiscall)) *NativeCycleIconConstructorFunction)(
    void *cycle_icon
);
typedef void *(__attribute__((thiscall)) *NativeCycleIconDestructorFunction)(
    void *cycle_icon,
    unsigned int deleting
);
typedef void (__attribute__((thiscall)) *UiTextColorFunction)(
    void *renderer,
    unsigned int layer,
    const float *color
);
typedef unsigned int (__attribute__((thiscall)) *AnimRendererNamedIndexFunction)(
    void *renderer,
    unsigned int name_hash
);
typedef void *(__attribute__((thiscall)) *AnimRendererSubmodelObjectFunction)(
    void *renderer,
    unsigned int submodel_index
);
typedef unsigned int (__attribute__((thiscall)) *AnimRendererCountFunction)(
    void *renderer
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
typedef void (__stdcall *UiSceneRenderFunction)(void *scene);
typedef ULONG (__stdcall *ComReleaseFunction)(void *object);

typedef enum SudekiMpPendingAction {
    SUDEKIMP_PENDING_NONE = 0,
    SUDEKIMP_PENDING_SPAWN = 1,
    SUDEKIMP_PENDING_REMOVE = 2
} SudekiMpPendingAction;

typedef enum SudekiMpCleanroomVoteKind {
    SUDEKIMP_CLEANROOM_VOTE_TRAVEL = 0,
    SUDEKIMP_CLEANROOM_VOTE_SAVE_BOOK = 1
} SudekiMpCleanroomVoteKind;

typedef struct SudekiMpCleanroomVoteSnapshot {
    BOOL active;
    SudekiMpCleanroomVoteKind kind;
    unsigned int state;
    uint32_t serial;
    uint32_t remaining_ms;
    uint8_t participant_mask;
    uint8_t accepted_mask;
    uint8_t cancelled_mask;
    char subject[64];
} SudekiMpCleanroomVoteSnapshot;

typedef struct SudekiMpSaveBookVoteInputRuntime {
    SudekiMpSaveBookVoteInputFence player_two;
    BOOL overlay_acknowledged;
    BOOL player_one_cancel_was_down;
} SudekiMpSaveBookVoteInputRuntime;

enum {
    RVA_CONTROLLER_UPDATE = 0x00027cf0u,
    RVA_CONTROLLER_UPDATE_VTABLE_SLOT = 0x002c9f60u,
    RVA_FRAME_END = 0x001dd540u,
    RVA_FRAME_END_CALL = 0x0028d58cu,
    RVA_FRONT_END_ACTION = 0x000a0360u,
    RVA_FRONT_END_ACTION_VTABLE_SLOT = 0x002cb228u,
    RVA_FRONT_END_VTABLE = 0x002cb1fcu,
    RVA_FRONT_END_UPDATE = 0x000a2ca0u,
    RVA_FRONT_END_UPDATE_VTABLE_SLOT = 0x002cb2bcu,
    RVA_FRONT_END_RENDER = 0x000a3760u,
    RVA_FRONT_END_RENDER_VTABLE_SLOT = 0x002cb2c0u,
    RVA_PC_FRONT_END_GLOBAL = 0x003c2f94u,
    RVA_PC_FRONT_END_VTABLE = 0x002cb2b4u,
    RVA_UI_SCENE_GLOBAL = 0x00408d1cu,
    RVA_UI_TEXT_SUBMIT = 0x00009930u,
    RVA_UI_SCENE_RENDER_CALL = 0x0000a760u,
    RVA_UI_SCENE_RENDER = 0x0000a820u,
    RVA_FRONT_END_MENU_BUILDER = 0x000a1950u,
    RVA_FRONT_END_STATE_UPDATE = 0x000a0f40u,
    RVA_FRONT_END_SELECTION_REFRESH = 0x000a16f0u,
    RVA_UI_STATE_REQUEST = 0x00120260u,
    RVA_UI_LAYER_SUBMENU_VTABLE = 0x002ca834u,
    RVA_OPTIONS_MENU_VTABLE = 0x002d1cb4u,
    RVA_LOAD_GAME_MENU_VTABLE = 0x002ca89cu,
    RVA_LOAD_GAME_PAGE_LEAVE = 0x00084260u,
    RVA_UI_LAYER_SUBMENU_INPUT = 0x0011f850u,
    RVA_UI_LAYER_SUBMENU_NOOP = 0x000a2900u,
    RVA_UI_ELEMENT_BAR_VTABLE = 0x002d9024u,
    RVA_OPTIONS_ROWS_INITIALIZE = 0x0011d4d0u,
    RVA_GAME_ALLOCATE = 0x002484fau,
    RVA_ANIMATED_TITLE_ROW_CONSTRUCTOR = 0x0011fb40u,
    RVA_ANIMATED_TITLE_ROW_DESTRUCTOR = 0x0011fcc0u,
    RVA_ANIMATED_TITLE_ROW_VTABLE = 0x002d1da8u,
    RVA_ANIM_OBJECT_RENDERER_VTABLE = 0x002df8ecu,
    RVA_ANIM_RENDERER_COMPONENT_COUNT = 0x0021bc80u,
    RVA_ANIM_RENDERER_COMPONENT_OBJECT = 0x0021bc90u,
    RVA_ANIM_RENDERER_NAMED_COMPONENT = 0x0021bce0u,
    RVA_ANIM_RENDERER_SUBMODEL_COUNT = 0x0021bb10u,
    RVA_MOVIE_PLAY = 0x00104d90u,
    RVA_MOVIE_STOP = 0x00104ea0u,
    RVA_D3D_DEVICE_GLOBAL = 0x003c31dcu,
    RVA_NATIVE_TEXTURE_CREATE = 0x001d92e0u,
    RVA_NATIVE_D3D_TEXTURE_VTABLE = 0x002dd7bcu,
    RVA_NATIVE_RESIDENT_D3D_TEXTURE_VTABLE = 0x002dd80cu,
    RVA_NATIVE_D3D_TEXTURE_GET_GPU_HANDLE = 0x001d6810u,
    RVA_HUD_PORTRAIT_RESOURCE_SELECT = 0x0015c070u,
    RVA_CHARACTER_TYPE_TO_PORTRAIT_ENUM = 0x0003f430u,
    RVA_HUD_PORTRAIT_RESOURCE_INDEX_TABLE = 0x002c2a94u,
    RVA_NATIVE_CYCLE_ICON_CONSTRUCTOR = 0x0015bd10u,
    RVA_NATIVE_CYCLE_ICON_BIND = 0x0015be70u,
    RVA_NATIVE_CYCLE_ICON_DESTRUCTOR = 0x0015bd70u,
    RVA_NATIVE_CYCLE_ICON_VTABLE = 0x002d8524u,
    RVA_NATIVE_CYCLE_ICON_SECONDARY_VTABLE = 0x002d8540u,
    RVA_NATIVE_CYCLE_ICON_VISIBILITY = 0x0015c020u,
    RVA_NATIVE_CYCLE_ICON_REFRESH = 0x0015c230u,
    RVA_SOL_MATERIAL_VTABLE = 0x002deb7cu,
    RVA_NATIVE_SAVE_ENTRY_UPDATE = 0x0008c710u,
    RVA_NATIVE_SAVE_PAGE_ACTION = 0x000898a0u,
    RVA_NATIVE_SAVE_PAGE_INPUT = 0x0008d970u,
    MENU_ACTOR_COUNT = 4u,
    MENU_DUMMY_INDEX = 4u,
    MENU_COMBAT_INDEX = 5u,
    MENU_CAMERA_INDEX = 6u,
    MENU_MULTIPLAYER_INDEX = 7u,
    MENU_INFINITE_SP_INDEX = 8u,
    MENU_INFINITE_SPIRIT_INDEX = 9u,
    MENU_INFINITE_JETPACK_INDEX = 10u,
    MENU_COOP_READY_INDEX = 11u,
    MENU_CLOSE_INDEX = 12u,
    MENU_ITEM_COUNT = 13u,
    MENU_TEXTURE_WIDTH = 640u,
    MENU_TEXTURE_HEIGHT = 480u,
    ROSTER_BACKDROP_WIDTH = 64u,
    ROSTER_BACKDROP_HEIGHT = 64u,
    PLAYER_TWO_BADGE_WIDTH = 168u,
    PLAYER_TWO_BADGE_HEIGHT = 42u,
    TRANSITION_VOTE_OVERLAY_WIDTH = 512u,
    TRANSITION_VOTE_OVERLAY_HEIGHT = 196u,
    ROAMING_BOUNDARY_OVERLAY_WIDTH = 640u,
    ROAMING_BOUNDARY_OVERLAY_HEIGHT = 480u,
    MENU_TIMEOUT_MS = 6000u,
    MENU_STATUS_INTERVAL_MS = 150u,
    ZONE_TRAVERSAL_PAGE_WORLDS = 0u,
    ZONE_TRAVERSAL_PAGE_INTERIORS = 1u,
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

enum {
    NATIVE_TITLE_ROW_COUNT = 5u,
    NATIVE_ROSTER_ROW_COUNT = 5u,
    ROSTER_CAPSULE_FIRST_TOP = 319u,
    ROSTER_CAPSULE_ROW_SPACING = 31u,
    ROSTER_CAPSULE_HEIGHT = 30u,
    ROSTER_CAPSULE_DRAW_TOP_MARGIN = 1u,
    ROSTER_CAPSULE_DRAW_BOTTOM_EXCLUSIVE = 4u,
    ROSTER_CAPSULE_MAX_DRAW_ROW =
        ROSTER_CAPSULE_FIRST_TOP +
        (NATIVE_TITLE_ROW_COUNT - 1u) * ROSTER_CAPSULE_ROW_SPACING +
        ROSTER_CAPSULE_HEIGHT + ROSTER_CAPSULE_DRAW_BOTTOM_EXCLUSIVE - 1u,
    NATIVE_ROSTER_FIRST_RESOURCE_ID = 0x63u,
    NATIVE_ANIMATED_TITLE_ROW_SIZE = 0x13cu,
    NATIVE_TITLE_ROW_POINTER_OFFSET = 0x70u,
    NATIVE_TITLE_LABEL_POINTER_OFFSET = 0x88u,
    NATIVE_UI_LABEL_RENDERER_OFFSET = 0x34u,
    NATIVE_UI_TEXT_COLOR_VTABLE_OFFSET = 0x2cu,
    NATIVE_UI_ROW_QUEUE_READ_OFFSET = 0xb4u,
    NATIVE_UI_ROW_QUEUE_WRITE_OFFSET = 0xb8u,
    NATIVE_UI_ROW_STATE_IDLE = 0u,
    NATIVE_UI_ROW_STATE_OFF = 2u,
    NATIVE_UI_ROW_STATE_HIGHLIGHT = 3u
};

_Static_assert(
    (unsigned int)ROSTER_CAPSULE_MAX_DRAW_ROW <
        (unsigned int)MENU_TEXTURE_HEIGHT,
    "five roster capsule rows must fit inside the menu texture"
);

/* The shipped title labels use font 1, alignment mode 1, and x=0x156.  That
 * exact path centers Continue Game/New Game on the title bars.  Mode 0 belongs
 * to a different general-UI layout path: it rendered our title text at the
 * right edge despite otherwise valid coordinates. */
enum {
    /* The title page submits its own labels with font 1.  Font 0 is the
     * smaller general UI face and uses a different layout context, which
     * places otherwise-correct roster coordinates off the title rows. */
    NATIVE_UI_TEXT_FONT_TITLE = 1u,
    /* Font-1 mode 1 is a left anchor.  The shipped title scene places its
     * local x=342 at the left edge of the stock labels, so roster text is
     * centered explicitly in that same local coordinate space. */
    NATIVE_UI_TEXT_ALIGNMENT_TITLE_LEFT = 1u,
    NATIVE_ROSTER_TITLE_CONTENT_CENTER_X = 320u,
    NATIVE_ROSTER_HEADING_Y = 276u,
    /* The generated row texture and the native title-font queue use
     * different vertical transforms.  This measured title-font coordinate
     * centers the label on the generated capsule whose first texture row is
     * ROSTER_CAPSULE_FIRST_TOP. */
    NATIVE_ROSTER_FIRST_ROW_Y = 300u,
    NATIVE_ROSTER_PROMPT_Y = 438u
};

enum {
    NATIVE_OPTIONS_ROW_POINTER_OFFSET = 0xa8u
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
static SudekiMpInlineHook native_save_entry_update_hook;
void *native_save_entry_update_trampoline;
static SudekiMpInlineHook native_save_page_action_hook;
void *native_save_page_action_trampoline;
static SudekiMpInlineHook native_save_page_input_hook;
void *native_save_page_input_trampoline;
static void *native_save_entry_last_object;
static unsigned int native_save_entry_last_mode = UINT_MAX;
static unsigned int native_save_entry_last_selection = UINT_MAX;
static void *native_save_page_action_last_object;
static unsigned int native_save_page_action_last_event = UINT_MAX;
static unsigned int native_save_page_action_last_argument = UINT_MAX;
static void *native_save_page_input_last_object;
static unsigned int native_save_page_input_last_event = UINT_MAX;
static unsigned int native_save_page_input_last_argument = UINT_MAX;
static BOOL native_save_page_input_suppression_logged;

BOOL should_suppress_native_save_page_input(void);
static SudekiMpRelativeCallHook frame_end_hook;
static SudekiMpRelativeCallHook roster_ui_scene_render_hook;
static SudekiMpInlineHook front_end_action_hook;
static SudekiMpPointerHook front_end_action_vtable_hook;
static SudekiMpInlineHook front_end_update_hook;
static SudekiMpInlineHook front_end_state_update_hook;
static SudekiMpInlineHook startup_movie_hook;
static SudekiMpPointerHook front_end_render_hook;
static ControllerUpdateFunction original_controller_update;
static FrameEndFunction original_frame_end;
static UiSceneRenderFunction original_ui_scene_render;
static FrontEndActionFunction original_front_end_action;
static FrontEndUpdateFunction original_front_end_update;
static FrontEndRenderFunction original_front_end_render;
FrontEndStateUpdateFunction original_front_end_state_update;
static MoviePlayFunction original_movie_play;
static FrontEndMenuBuilderFunction front_end_menu_builder;
static FrontEndSelectionRefreshFunction front_end_selection_refresh;
static uint8_t *game_base;
static HANDLE pc_front_end_trace_thread_handle;
static volatile LONG pc_front_end_trace_stop;
static void **d3d_device_global;
static BOOL menu_memory_readable(const void *pointer, size_t size);

BOOL SudekiMpCleanroomNativeSaveModalActive(void) {
    return SudekiMpSplitScreenNativeSaveModalActive();
}

void trace_native_save_entry_update(void *entry) {
    unsigned int mode;
    unsigned int selection;
    void *state;
    void *party_data = NULL;

    if (entry == NULL || !menu_memory_readable(entry, 0x5cu)) {
        return;
    }
    mode = *(unsigned int *)((uint8_t *)entry + 0x48u);
    selection = *(unsigned int *)((uint8_t *)entry + 0x54u);
    state = *(void **)((uint8_t *)entry + 0x58u);
    if (menu_memory_readable(state, 0x58u)) {
        party_data = *(void **)((uint8_t *)state + 0x54u);
    }
    if (entry == native_save_entry_last_object &&
        mode == native_save_entry_last_mode &&
        selection == native_save_entry_last_selection) {
        return;
    }
    native_save_entry_last_object = entry;
    native_save_entry_last_mode = mode;
    native_save_entry_last_selection = selection;
    SudekiMpLogFormat(
        "cleanroom_menu event=native_save_entry_update object=%p mode=%lu "
        "selection=%lu state=%p party_data=%p policy=observation_only "
        "rva=0x%08lx\r\n",
        entry,
        (unsigned long)mode,
        (unsigned long)selection,
        state,
        party_data,
        (unsigned long)RVA_NATIVE_SAVE_ENTRY_UPDATE
    );
}

void trace_native_save_page_action(
    void *controller,
    unsigned int event,
    unsigned int argument
) {
    unsigned int state = UINT_MAX;
    unsigned int selection = UINT_MAX;
    unsigned int mode = UINT_MAX;

    if (controller == NULL || !menu_memory_readable(controller, 0x8cu)) {
        return;
    }
    mode = *(unsigned int *)((uint8_t *)controller + 0x88u);
    selection = *(unsigned int *)((uint8_t *)controller + 0x54u);
    state = *(unsigned int *)((uint8_t *)controller + 0x48u);
    if (controller == native_save_page_action_last_object &&
        event == native_save_page_action_last_event &&
        argument == native_save_page_action_last_argument) {
        return;
    }
    native_save_page_action_last_object = controller;
    native_save_page_action_last_event = event;
    native_save_page_action_last_argument = argument;
    SudekiMpLogFormat(
        "cleanroom_menu event=native_save_page_action controller=%p "
        "event_code=%lu argument=%lu mode=%lu state=%lu "
        "selection=%lu policy=observation_only "
        "rva=0x%08lx\r\n",
        controller,
        (unsigned long)event,
        (unsigned long)argument,
        (unsigned long)mode,
        (unsigned long)state,
        (unsigned long)selection,
        (unsigned long)RVA_NATIVE_SAVE_PAGE_ACTION
    );
}

void trace_native_save_page_input(
    void *controller,
    unsigned int event,
    unsigned int argument
) {
    unsigned int page_kind = UINT_MAX;
    unsigned int focus = UINT_MAX;
    unsigned int selected = UINT_MAX;

    if (controller == NULL || !menu_memory_readable(controller, 0x318u)) {
        return;
    }
    page_kind = *(unsigned int *)((uint8_t *)controller + 0x314u);
    focus = *(unsigned int *)((uint8_t *)controller + 0x64u);
    selected = *(unsigned int *)((uint8_t *)controller + 0x2f0u);
    if (controller == native_save_page_input_last_object &&
        event == native_save_page_input_last_event &&
        argument == native_save_page_input_last_argument) {
        return;
    }
    native_save_page_input_last_object = controller;
    native_save_page_input_last_event = event;
    native_save_page_input_last_argument = argument;
    SudekiMpLogFormat(
        "cleanroom_menu event=native_save_page_input controller=%p "
        "event_code=%lu argument=%lu page_kind=%lu focus=%lu "
        "selected=%lu policy=observation_only rva=0x%08lx\r\n",
        controller,
        (unsigned long)event,
        (unsigned long)argument,
        (unsigned long)page_kind,
        (unsigned long)focus,
        (unsigned long)selected,
        (unsigned long)RVA_NATIVE_SAVE_PAGE_INPUT
    );
}

__attribute__((naked, noinline, used))
static void native_save_entry_update_entry(void) {
    __asm__ volatile(
        "pushal\n\t"
        "pushl 28(%esp)\n\t"
        "call _trace_native_save_entry_update\n\t"
        "addl $4, %esp\n\t"
        "popal\n\t"
        "jmp *_native_save_entry_update_trampoline\n\t"
    );
}

__attribute__((naked, noinline, used))
static void native_save_page_action_entry(void) {
    __asm__ volatile(
        "pushal\n\t"
        "pushl 40(%esp)\n\t"
        "pushl 40(%esp)\n\t"
        "pushl 32(%esp)\n\t"
        "call _trace_native_save_page_action\n\t"
        "addl $12, %esp\n\t"
        "call _should_suppress_native_save_page_input\n\t"
        "movl %eax, 28(%esp)\n\t"
        "popal\n\t"
        "testl %eax, %eax\n\t"
        "jnz 1f\n\t"
        "jmp *_native_save_page_action_trampoline\n\t"
        "1:\n\t"
        "movl $1, %eax\n\t"
        "ret $8\n\t"
    );
}

__attribute__((naked, noinline, used))
static void native_save_page_input_entry(void) {
    __asm__ volatile(
        "pushal\n\t"
        "pushl 40(%esp)\n\t"
        "pushl 40(%esp)\n\t"
        "pushl 32(%esp)\n\t"
        "call _trace_native_save_page_input\n\t"
        "addl $12, %esp\n\t"
        "call _should_suppress_native_save_page_input\n\t"
        "movl %eax, 28(%esp)\n\t"
        "popal\n\t"
        "testl %eax, %eax\n\t"
        "jnz 1f\n\t"
        "jmp *_native_save_page_input_trampoline\n\t"
        "1:\n\t"
        "movl $1, %eax\n\t"
        "ret $8\n\t"
    );
}

static void *menu_texture;
static void *menu_texture_device;
static void *player_two_badge_texture;
static void *player_two_badge_device;
static BOOL player_two_badge_dirty;
static void *transition_vote_texture;
static void *transition_vote_texture_device;
static BOOL transition_vote_texture_dirty;
static BOOL transition_vote_overlay_failure_logged;
static BOOL save_book_vote_overlay_failure_logged;
static uint32_t transition_vote_overlay_serial;
static uint32_t transition_vote_overlay_tenth_seconds;
static unsigned int transition_vote_overlay_state;
static unsigned int transition_vote_overlay_kind;
static uint8_t transition_vote_overlay_participant_mask;
static uint8_t transition_vote_overlay_accepted_mask;
static char transition_vote_overlay_destination[64];
static SudekiMpSaveBookVoteInputRuntime save_book_vote_input;
static void *roaming_boundary_texture;
static void *roaming_boundary_texture_device;
static BOOL roaming_boundary_texture_dirty;
static BOOL roaming_boundary_overlay_failure_logged;
static unsigned int roaming_boundary_overlay_phase;
static unsigned int roaming_boundary_overlay_percent;
static void *blacksmith_ui_texture;
static void *blacksmith_ui_texture_device;
static BOOL blacksmith_ui_texture_dirty;
static BOOL blacksmith_ui_overlay_failure_logged;
static SudekiMpBlacksmithUiSnapshot blacksmith_ui_last_snapshot;
static void *roster_button_texture;
static void *roster_button_texture_device;
static BOOL roster_button_texture_dirty;
static void *roster_backdrop_texture;
static void *roster_backdrop_texture_device;
static void *roster_native_portrait_textures[MENU_ACTOR_COUNT];
static void *roster_native_portrait_gpu_textures[MENU_ACTOR_COUNT];
static void *roster_native_portrait_icons[MENU_ACTOR_COUNT];
static void *roster_native_portrait_owner;
static void *roster_native_portrait_controller;
static BOOL roster_native_portraits_requested;
static BOOL roster_native_portraits_borrowed;
static BOOL roster_native_portrait_request_failed;
static UINT menu_toggle_key;
static BOOL menu_open;
static BOOL menu_texture_dirty;
static unsigned int selected_item;
static BOOL key_was_down[8];
static BOOL story_test_boost_enabled;
static BOOL story_test_boost_active;
static BOOL story_test_boost_runtime_applied;
static BOOL story_test_boost_key_was_down;
static BOOL story_test_boost_failure_logged;
static UINT story_test_boost_key;
static float story_test_boost_multiplier;
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
static BOOL infinite_jetpack_fuel;
static BOOL infinite_jetpack_fuel_valid;
static BOOL integrated_multiplayer_mode;
static BOOL zone_traversal_mode;
static unsigned int zone_traversal_page;
static unsigned int zone_traversal_selection;
static BOOL zone_traversal_waiting;
static char zone_traversal_waiting_world[64];
static DWORD zone_traversal_waiting_since;
static DWORD zone_traversal_transition_guard_until;
/*
 * EnterTemporaryZone is not a complete arbitrary-area teleport by itself:
 * native doors also establish the destination start-position/camera context.
 * Keep direct menu activation disabled until that authored context seam is
 * identified; a raw call can place the party in the skybox and is unsafe to
 * repeat while the asynchronous load is still settling.
 */
static const BOOL zone_traversal_direct_temporary_enabled = TRUE;
/* SetZoneNOW performs world teardown/load, but the cleanroom has no authored
 * default spawn/camera handoff for the newly selected world.  Keep direct
 * persistent-world jumps fail-closed until that context is identified. */
static const BOOL zone_traversal_direct_persistent_enabled = TRUE;

typedef struct SudekiMpTraversalWorld {
    const char *name;
    const char *label;
} SudekiMpTraversalWorld;

typedef struct SudekiMpTraversalInterior {
    const char *world;
    const char *name;
    const char *label;
} SudekiMpTraversalInterior;

static const SudekiMpTraversalWorld traversal_worlds[] = {
    {"NewBrightwater", "NEWBRIGHTWATER"},
    {"Illumina_Countryside_Hub", "COUNTRYSIDE HUB"},
    {"Illumina_Countryside_NE", "COUNTRYSIDE NE"},
    {"Illumina_Countryside_SE", "COUNTRYSIDE SE"},
    {"Illumina_Countryside_SW", "COUNTRYSIDE SW"},
    {"Illumina_Countryside_NW", "COUNTRYSIDE NW"}
};

static const SudekiMpTraversalInterior traversal_interiors[] = {
    {"NewBrightwater", "LNBr_Church", "CHURCH"},
    {"NewBrightwater", "LNBr_Kamo_shop", "KAMO SHOP"},
    {"NewBrightwater", "LNBr_Kilks_house", "KILKS HOUSE"},
    {"NewBrightwater", "LNBr_Lighthouse", "LIGHTHOUSE"},
    {"NewBrightwater", "LNBr_Salty_dog_Inn", "SALTY DOG INN"},
    {"NewBrightwater", "LNBr_ShortTent", "SHORT TENT"},
    {"NewBrightwater", "LNBr_TallTent01", "TALL TENT 01"},
    {"NewBrightwater", "LNBr_TallTent02", "TALL TENT 02"},
    {"Illumina_Countryside_SE", "LICo_Athlos_Shack", "ATHLOS SHACK"},
    {"Illumina_Countryside_SE", "LICo_Frappe_Farm", "FRAPPE FARM"},
    {"Illumina_Countryside_SE", "LICo_Porkins", "PORKINS"},
    {"Illumina_Countryside_SE", "LICo_SW_Trader_Cave", "TRADER CAVE"}
};
static BOOL roster_mode;
static BOOL roster_locked;
static BOOL loaded_save_coop_autostart_enabled;
static BOOL loaded_save_coop_autostart_roster_published;
static BOOL loaded_save_coop_autostart_last_locked;
static BOOL roster_coop_profile;
static BOOL roster_talos_tuning_enabled;
static unsigned int roster_talos_health_scale;
static unsigned int roster_talos_stagger_limit;
static unsigned int roster_talos_stagger_window;
static unsigned int roster_player_one;
static unsigned int roster_player_two;
static unsigned int roster_cursor;
static BOOL multiplayer_requested;
static BOOL multiplayer_active;
static BOOL multiplayer_input_ready;
static BOOL multiplayer_participation_requested;
static BOOL coop_role_lock_active;
static SudekiMpCleanroomActor coop_selected_actor;
static DWORD coop_ready_failed_until;
static BOOL coop_lobby_prompted;
static DWORD last_status_update;
static BOOL roster_waiting_new_game;
static BOOL roster_replaying_new_game;
static BOOL roster_resume_committed;
static void *roster_resume_controller;
static BOOL roster_native_screen;
static unsigned int roster_native_screen_kind;
static unsigned int roster_native_selection;
static BOOL roster_native_screen_dirty;
static BOOL roster_confirm_input_armed;
static DWORD roster_native_transition_started;
static BOOL roster_native_transition_from_title;
static void *roster_pending_controller;
static unsigned int roster_pending_phase;
static unsigned int roster_pending_event;
static unsigned int roster_pending_argument;
static BOOL roster_original_menu_valid;
static unsigned int roster_original_menu_count;
static unsigned int roster_original_menu_selection;
static uint8_t roster_original_menu_labels[8u][0x3cu];
static uint8_t roster_original_menu_actions[8u][0x20u];
static void *roster_native_label_objects[NATIVE_TITLE_ROW_COUNT];
static BOOL roster_native_label_states_valid;
static BOOL roster_native_rows_active;
static uint8_t roster_native_page_object[0x114u];
static void *roster_native_page_vtable[24u];
static BOOL roster_native_page_contract_ready;
static BOOL roster_native_page_state_active;
static BOOL roster_native_page_takeover_pending;
static BOOL roster_native_page_leave_requested;
static DWORD roster_native_page_leave_started_ms;
static void *roster_native_page_controller;
static void *roster_native_page_saved_active;
static void *roster_native_page_saved_backing;
static unsigned int roster_native_page_saved_state;
static unsigned int roster_native_page_saved_previous;
static unsigned int roster_native_page_saved_mode;
static void *roster_native_options_rows[NATIVE_TITLE_ROW_COUNT];
static BOOL roster_native_options_rows_bound;
static void *roster_native_animated_rows[NATIVE_ROSTER_ROW_COUNT];
static BOOL roster_native_animated_rows_owned;
static char roster_persistence_path[MAX_PATH];

/* State 6 temporarily activates Sudeki's real Load Game page so its resident
 * UI scene can create ICON_PORTRAIT1..4.  The confirming key that opened our
 * roster can otherwise reach the save page during that single initialization
 * tick and open "Load this saved game?" behind the custom page.  Both native
 * save input dispatchers are exact-image hooks with the same thiscall/ret-8
 * contract; consume their events only while SudekiMP owns this roster state.
 */
BOOL __attribute__((noinline, used))
should_suppress_native_save_page_input(void) {
    BOOL suppress = roster_native_screen &&
        roster_native_page_state_active;

    if (suppress && !native_save_page_input_suppression_logged) {
        native_save_page_input_suppression_logged = TRUE;
        SudekiMpLogWrite(
            "cleanroom_menu event=native_save_page_input_suppression "
            "status=active scope=roster_owned_state6 policy=consume_only\r\n");
    }
    return suppress;
}

enum {
    NATIVE_ROSTER_NONE = 0u,
    NATIVE_ROSTER_MODE = 1u,
    NATIVE_ROSTER_PLAYER_ONE = 2u,
    NATIVE_ROSTER_PLAYER_TWO = 3u,
    NATIVE_ROSTER_CONFIRM = 4u,
    NATIVE_ROSTER_SETTINGS = 5u
};

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

static BOOL menu_memory_readable(const void *pointer, size_t size) {
    MEMORY_BASIC_INFORMATION information;
    uintptr_t start;
    uintptr_t end;
    uintptr_t region_end;

    if (pointer == NULL || size == 0u ||
        VirtualQuery(pointer, &information, sizeof(information)) == 0 ||
        information.State != MEM_COMMIT ||
        (information.Protect & (PAGE_NOACCESS | PAGE_GUARD)) != 0u) {
        return FALSE;
    }
    start = (uintptr_t)pointer;
    end = start + size;
    region_end = (uintptr_t)information.BaseAddress + information.RegionSize;
    return end >= start && end <= region_end;
}

static BOOL menu_memory_executable(const void *pointer) {
    MEMORY_BASIC_INFORMATION information;
    DWORD protection;

    if (pointer == NULL ||
        VirtualQuery(pointer, &information, sizeof(information)) == 0 ||
        information.State != MEM_COMMIT ||
        (information.Protect & (PAGE_NOACCESS | PAGE_GUARD)) != 0u) {
        return FALSE;
    }
    protection = information.Protect & 0xffu;
    return protection == PAGE_EXECUTE ||
        protection == PAGE_EXECUTE_READ ||
        protection == PAGE_EXECUTE_READWRITE ||
        protection == PAGE_EXECUTE_WRITECOPY;
}

static unsigned int __attribute__((thiscall))
native_roster_page_input_noop(
    void *page,
    unsigned int phase,
    unsigned int event,
    unsigned int argument
) {
    (void)page;
    (void)phase;
    (void)event;
    (void)argument;
    return 1u;
}

static unsigned int __attribute__((thiscall))
native_roster_page_is_closed(void *page) {
    (void)page;
    return 0u;
}

static void __attribute__((thiscall))
native_roster_page_activate(void *page) {
    (void)page;
    SudekiMpLogWrite(
        "cleanroom_menu event=native_roster_page state=activated "
        "contract=UILayerSubMenu_compatible\r\n");
}

static void __attribute__((thiscall))
native_roster_page_release(void *page) {
    (void)page;
    SudekiMpLogWrite(
        "cleanroom_menu event=native_roster_page state=released\r\n");
}

static BOOL native_roster_prepare_page_contract(void) {
    void **native_vtable;

    if (roster_native_page_contract_ready) {
        return TRUE;
    }
    if (game_base == NULL) {
        return FALSE;
    }
    native_vtable = (void **)(game_base + RVA_UI_LAYER_SUBMENU_VTABLE);
    if (!menu_memory_readable(native_vtable,
            sizeof(roster_native_page_vtable)) ||
        native_vtable[0x2cu / sizeof(void *)] !=
            game_base + RVA_UI_LAYER_SUBMENU_INPUT ||
        native_vtable[0x48u / sizeof(void *)] !=
            game_base + RVA_UI_LAYER_SUBMENU_NOOP ||
        native_vtable[0x4cu / sizeof(void *)] !=
            game_base + RVA_UI_LAYER_SUBMENU_NOOP) {
        SudekiMpLogWrite(
            "cleanroom_menu event=native_roster_page status=rejected "
            "reason=UILayerSubMenu_vtable_gate_failed\r\n");
        return FALSE;
    }
    memcpy(roster_native_page_vtable, native_vtable,
        sizeof(roster_native_page_vtable));
    roster_native_page_vtable[0x2cu / sizeof(void *)] =
        (void *)native_roster_page_input_noop;
    roster_native_page_vtable[0x34u / sizeof(void *)] =
        (void *)native_roster_page_is_closed;
    roster_native_page_vtable[0x48u / sizeof(void *)] =
        (void *)native_roster_page_activate;
    roster_native_page_vtable[0x4cu / sizeof(void *)] =
        (void *)native_roster_page_release;
    ZeroMemory(roster_native_page_object,
        sizeof(roster_native_page_object));
    *(void **)&roster_native_page_object[0] = roster_native_page_vtable;
    roster_native_page_contract_ready = TRUE;
    return TRUE;
}

static unsigned int roster_actor_type(unsigned int actor) {
    static const unsigned int types[MENU_ACTOR_COUNT] = {
        0x23u, 0x05u, 0x0eu, 0x01u
    };
    return actor < MENU_ACTOR_COUNT ? types[actor] : 0u;
}

void SudekiMpCleanroomMenuSetLoadedSaveCoopAutostart(BOOL enabled) {
    loaded_save_coop_autostart_enabled = enabled ? TRUE : FALSE;
    loaded_save_coop_autostart_roster_published = FALSE;
    loaded_save_coop_autostart_last_locked = FALSE;
    SudekiMpLogFormat(
        "cleanroom_menu event=loaded_save_coop_autostart state=%s "
        "player_one=Tal player_two=Ailish "
        "policy=game_thread_after_world_and_party_settle\r\n",
        loaded_save_coop_autostart_enabled ? "armed" : "disabled");
}

static void service_loaded_save_coop_autostart(void) {
    BOOL roles_locked;

    if (!loaded_save_coop_autostart_enabled || !roster_mode) {
        return;
    }
    if (!SudekiMpCleanroomEngineWorldReady()) {
        return;
    }
    if (!loaded_save_coop_autostart_roster_published) {
        loaded_save_coop_autostart_roster_published =
            SudekiMpSplitScreenSetRosterTypes(
                roster_actor_type(SUDEKIMP_CLEANROOM_TAL),
                roster_actor_type(SUDEKIMP_CLEANROOM_AILISH));
        SudekiMpLogFormat(
            "cleanroom_menu event=loaded_save_coop_autostart "
            "phase=publish_roster status=%s player_one=Tal player_two=Ailish "
            "policy=defer_until_native_party_contains_both\r\n",
            loaded_save_coop_autostart_roster_published ?
                "accepted" : "rejected");
        if (!loaded_save_coop_autostart_roster_published) {
            return;
        }
    }
    /* This update hook is the existing native controller/game-thread seam.
     * ApplyRoster retains all rotation, AI override, and role-lock checks, so
     * a not-yet-settled save simply waits rather than being forced. */
    SudekiMpSplitScreenApplyRosterOnGameThread();
    roles_locked = SudekiMpSplitScreenRolesLocked();
    if (roles_locked != loaded_save_coop_autostart_last_locked) {
        loaded_save_coop_autostart_last_locked = roles_locked;
        coop_role_lock_active = roles_locked;
        SudekiMpLogFormat(
            "cleanroom_menu event=loaded_save_coop_autostart phase=%s "
            "policy=atomic_native_roster_apply\r\n",
            roles_locked ? "active" : "waiting_for_settled_party");
    }
}

static const char *roster_actor_label(unsigned int actor) {
    return actor < MENU_ACTOR_COUNT ?
        SudekiMpCleanroomActorLabel((SudekiMpCleanroomActor)actor) :
        "Unknown";
}

/* The cleanroom/engine enum follows the game's internal order
 * Tal/Buki/Elco/Ailish.  The title roster deliberately presents the familiar
 * player-facing order Ailish/Tal/Buki/Elco.  Keep that translation explicit:
 * every stored roster choice must be an engine actor, never a card index. */
static SudekiMpCleanroomActor roster_display_actor(unsigned int card) {
    static const SudekiMpCleanroomActor display_order[MENU_ACTOR_COUNT] = {
        SUDEKIMP_CLEANROOM_AILISH,
        SUDEKIMP_CLEANROOM_TAL,
        SUDEKIMP_CLEANROOM_BUKI,
        SUDEKIMP_CLEANROOM_ELCO
    };

    return card < MENU_ACTOR_COUNT ? display_order[card] :
        SUDEKIMP_CLEANROOM_AILISH;
}

static const char *roster_display_actor_label(unsigned int card) {
    return roster_actor_label((unsigned int)roster_display_actor(card));
}

static unsigned int roster_display_card_for_actor(unsigned int actor) {
    unsigned int card;

    for (card = 0u; card < MENU_ACTOR_COUNT; ++card) {
        if ((unsigned int)roster_display_actor(card) == actor) {
            return card;
        }
    }
    return 0u;
}

static unsigned int roster_first_available_player_two_card(void) {
    unsigned int card;

    for (card = 0u; card < MENU_ACTOR_COUNT; ++card) {
        if ((unsigned int)roster_display_actor(card) != roster_player_one) {
            return card;
        }
    }
    return 4u;
}

static unsigned int roster_actor_from_label(const char *label) {
    unsigned int actor;

    if (label == NULL) {
        return SUDEKIMP_CLEANROOM_AILISH;
    }
    for (actor = 0u; actor < MENU_ACTOR_COUNT; ++actor) {
        if (_stricmp(label, roster_actor_label(actor)) == 0) {
            return actor;
        }
    }
    return SUDEKIMP_CLEANROOM_AILISH;
}

static void roster_build_persistence_path(void) {
    DWORD length = GetModuleFileNameA(NULL, roster_persistence_path,
        sizeof(roster_persistence_path));
    char *separator;

    if (length == 0u || length >= sizeof(roster_persistence_path)) {
        roster_persistence_path[0] = '\0';
        return;
    }
    separator = strrchr(roster_persistence_path, '\\');
    if (separator == NULL) {
        separator = strrchr(roster_persistence_path, '/');
    }
    if (separator == NULL ||
        (size_t)(separator - roster_persistence_path) + 1u +
            strlen("SudekiMP-roster.ini") >= sizeof(roster_persistence_path)) {
        roster_persistence_path[0] = '\0';
        return;
    }
    separator[1] = '\0';
    lstrcatA(roster_persistence_path, "SudekiMP-roster.ini");
}

static void roster_load_persistence(void) {
    char player_one[32];
    char player_two[32];
    char mode[16];

    if (roster_persistence_path[0] == '\0') {
        return;
    }
    GetPrivateProfileStringA("Roster", "PlayerOne", "Ailish", player_one,
        sizeof(player_one), roster_persistence_path);
    GetPrivateProfileStringA("Roster", "PlayerTwo", "Tal", player_two,
        sizeof(player_two), roster_persistence_path);
    GetPrivateProfileStringA("Roster", "Mode", "Single", mode,
        sizeof(mode), roster_persistence_path);
    roster_player_one = roster_actor_from_label(player_one);
    roster_player_two = roster_actor_from_label(player_two);
    if (roster_player_one == roster_player_two) {
        roster_player_one = SUDEKIMP_CLEANROOM_AILISH;
        roster_player_two = SUDEKIMP_CLEANROOM_TAL;
    }
    roster_coop_profile = _stricmp(mode, "Coop") == 0;
    roster_talos_tuning_enabled = GetPrivateProfileIntA(
        "TalosCoop", "Enabled", 0, roster_persistence_path) != 0;
    /* The settings page must also work with an existing late-game save.  An
     * explicit Talos tuning opt-in therefore establishes the sidecar's co-op
     * profile even when an older build left Mode=Single behind.  Choosing the
     * visible Single Player action below disables both values again. */
    if (roster_talos_tuning_enabled) {
        roster_coop_profile = TRUE;
    }
    roster_talos_health_scale = (unsigned int)GetPrivateProfileIntA(
        "TalosCoop", "HealthScale", 2, roster_persistence_path);
    roster_talos_stagger_limit = (unsigned int)GetPrivateProfileIntA(
        "TalosCoop", "StaggerLimit", 10, roster_persistence_path);
    roster_talos_stagger_window = (unsigned int)GetPrivateProfileIntA(
        "TalosCoop", "StaggerWindowSeconds", 10,
        roster_persistence_path);
    if (roster_talos_health_scale < 1u ||
        roster_talos_health_scale > 4u) {
        roster_talos_health_scale = 2u;
    }
    if (roster_talos_stagger_limit != 6u &&
        roster_talos_stagger_limit != 10u &&
        roster_talos_stagger_limit != 14u &&
        roster_talos_stagger_limit != 18u) {
        roster_talos_stagger_limit = 10u;
    }
    if (roster_talos_stagger_window != 5u &&
        roster_talos_stagger_window != 10u &&
        roster_talos_stagger_window != 15u &&
        roster_talos_stagger_window != 20u) {
        roster_talos_stagger_window = 10u;
    }
    (void)SudekiMpTalosCoopBalanceConfigure(
        roster_talos_tuning_enabled,
        roster_coop_profile,
        roster_talos_health_scale,
        roster_talos_stagger_limit,
        roster_talos_stagger_window);
    /* Loading the sidecar is also the authoritative way to restore the
     * gameplay contract after the user cancels out of the New Game editor.
     * The editor deliberately clears roster_locked while its choices are
     * provisional; a Single profile must therefore clear an older in-memory
     * co-op lock just as a Coop profile republishes it. */
    roster_locked = FALSE;
    if (roster_coop_profile) {
        roster_locked = SudekiMpSplitScreenSetRosterTypes(
            roster_actor_type(roster_player_one),
            roster_actor_type(roster_player_two));
    } else if (!SudekiMpSplitScreenClearRosterTypes()) {
        SudekiMpLogFormat(
            "cleanroom_menu event=native_roster_persistence status=rejected "
            "reason=single_profile_runtime_release_failed error=%lu\r\n",
            (unsigned long)GetLastError());
    }
    SudekiMpLogFormat(
        "cleanroom_menu event=native_roster_persistence status=loaded "
        "path=%s p1=%s p2=%s mode=%s talos_tuning=%s "
        "talos_health=%ux talos_stagger=%u/%us policy=sidecar_profile\r\n",
        roster_persistence_path,
        roster_actor_label(roster_player_one),
        roster_actor_label(roster_player_two),
        roster_coop_profile ? "Coop" : "Single",
        roster_talos_tuning_enabled ? "on" : "off",
        roster_talos_health_scale,
        roster_talos_stagger_limit,
        roster_talos_stagger_window
    );
}

static void roster_save_persistence(void) {
    if (roster_persistence_path[0] == '\0') {
        return;
    }
    WritePrivateProfileStringA("Roster", "PlayerOne",
        roster_actor_label(roster_player_one), roster_persistence_path);
    WritePrivateProfileStringA("Roster", "PlayerTwo",
        roster_actor_label(roster_player_two), roster_persistence_path);
    WritePrivateProfileStringA("Roster", "Mode",
        roster_coop_profile ? "Coop" : "Single", roster_persistence_path);
    WritePrivateProfileStringA("TalosCoop", "Enabled",
        roster_talos_tuning_enabled ? "1" : "0", roster_persistence_path);
    {
        char value[16];
        wsprintfA(value, "%u", roster_talos_health_scale);
        WritePrivateProfileStringA(
            "TalosCoop", "HealthScale", value, roster_persistence_path);
        wsprintfA(value, "%u", roster_talos_stagger_limit);
        WritePrivateProfileStringA(
            "TalosCoop", "StaggerLimit", value, roster_persistence_path);
        wsprintfA(value, "%u", roster_talos_stagger_window);
        WritePrivateProfileStringA("TalosCoop", "StaggerWindowSeconds",
            value, roster_persistence_path);
    }
    (void)SudekiMpTalosCoopBalanceConfigure(
        roster_talos_tuning_enabled,
        roster_coop_profile,
        roster_talos_health_scale,
        roster_talos_stagger_limit,
        roster_talos_stagger_window);
    SudekiMpLogFormat(
        "cleanroom_menu event=native_roster_persistence status=saved "
        "path=%s p1=%s p2=%s mode=%s talos_tuning=%s "
        "talos_health=%ux talos_stagger=%u/%us policy=sidecar_profile\r\n",
        roster_persistence_path,
        roster_actor_label(roster_player_one),
        roster_actor_label(roster_player_two),
        roster_coop_profile ? "Coop" : "Single",
        roster_talos_tuning_enabled ? "on" : "off",
        roster_talos_health_scale,
        roster_talos_stagger_limit,
        roster_talos_stagger_window
    );
}

static void native_menu_set_utf16_string(uint8_t *record, const char *value) {
    size_t length = value == NULL ? 0u : strlen(value);
    size_t index;
    uint16_t *inline_text;

    if (length > 27u) {
        return;
    }
    *(uint32_t *)record = 0x80000000u | (uint32_t)length;
    inline_text = (uint16_t *)(record + 4u);
    for (index = 0u; index < length; ++index) {
        inline_text[index] = (uint16_t)(unsigned char)value[index];
    }
    inline_text[length] = 0u;
}

/* FUN_00409930 is Sudeki's own queued text submission helper.  It takes the
 * CUIScene in ECX, a native UTF-16 SSO record in EAX, and five stack
 * arguments.  Keeping the register bridge here lets the roster page use the
 * shipped PC font/layout path instead of the cleanroom D3D overlay. */
__attribute__((naked, noinline, used))
static void native_ui_submit_text_bridge(
    void *scene,
    void *text,
    unsigned int font,
    unsigned int alignment,
    unsigned int x,
    unsigned int y,
    unsigned int color,
    void *submit
) {
    __asm__ volatile(
        "pushl %ebp\n\t"
        "movl %esp, %ebp\n\t"
        "pushl 32(%ebp)\n\t"
        "pushl 28(%ebp)\n\t"
        "pushl 24(%ebp)\n\t"
        "pushl 20(%ebp)\n\t"
        "pushl 16(%ebp)\n\t"
        "movl 12(%ebp), %eax\n\t"
        "movl 8(%ebp), %ecx\n\t"
        "call *36(%ebp)\n\t"
        "popl %ebp\n\t"
        "ret\n\t"
    );
}

/* FUN_004A0F40 receives the requested native title state in EAX and the
 * title controller as its one callee-cleaned stack argument.  State 10 is
 * the shipped New Game fade-out presentation.  Calling it without changing
 * controller+0x44 lets us reuse that presentation while keeping the resident
 * title controller alive for the roster page. */
__attribute__((naked, noinline, used))
static void native_front_end_state_bridge(
    void *controller,
    unsigned int state,
    void *target
) {
    __asm__ volatile(
        "pushl %ebp\n\t"
        "movl %esp, %ebp\n\t"
        "pushl 8(%ebp)\n\t"
        "movl 12(%ebp), %eax\n\t"
        "call *16(%ebp)\n\t"
        "popl %ebp\n\t"
        "ret\n\t"
    );
}

/* UIElementCycleIcon::Bind uses Sudeki's register ABI: EAX owns the live
 * front-end UI resource context while four callee-cleaned arguments describe
 * the icon, element name, scene-node name, and decimal substitution. */
__attribute__((naked, noinline, used))
static void native_cycle_icon_bind_bridge(
    void *cycle_icon,
    const char *element_name,
    const char *scene_name,
    unsigned int index,
    void *owner,
    void *target
) {
    (void)cycle_icon;
    (void)element_name;
    (void)scene_name;
    (void)index;
    (void)owner;
    (void)target;
    __asm__ volatile(
        "pushl %ebp\n\t"
        "movl %esp, %ebp\n\t"
        "pushl 20(%ebp)\n\t"
        "pushl 16(%ebp)\n\t"
        "pushl 12(%ebp)\n\t"
        "pushl 8(%ebp)\n\t"
        "movl 24(%ebp), %eax\n\t"
        "call *28(%ebp)\n\t"
        "popl %ebp\n\t"
        "ret\n\t"
    );
}

__attribute__((naked, noinline, used))
static unsigned int native_portrait_enum_bridge(
    unsigned int character_type,
    void *target
) {
    (void)character_type;
    (void)target;
    __asm__ volatile(
        "movl 4(%esp), %eax\n\t"
        "call *8(%esp)\n\t"
        "ret\n\t"
    );
}

/* FUN_0055C070 consumes its one stack argument.  ECX selects the resource
 * table entry and EAX=1 requests the same synchronous completion used by the
 * accepted gameplay-HUD portrait path. */
__attribute__((naked, noinline, used))
static void native_cycle_icon_assign_bridge(
    void *cycle_icon,
    unsigned int resource_index,
    void *target
) {
    (void)cycle_icon;
    (void)resource_index;
    (void)target;
    __asm__ volatile(
        "movl 8(%esp), %ecx\n\t"
        "movl 12(%esp), %edx\n\t"
        "pushl 4(%esp)\n\t"
        "movl $1, %eax\n\t"
        "call *%edx\n\t"
        "ret\n\t"
    );
}

static BOOL native_roster_enter_page_state(void *controller) {
    void *load_game_page;
    void *active_page;

    if (controller == NULL || original_front_end_state_update == NULL ||
        !native_roster_prepare_page_contract() ||
        !menu_memory_readable(controller, 0xb8u)) {
        return FALSE;
    }
    load_game_page = *(void **)((uint8_t *)controller + 0xb4u);
    if (!menu_memory_readable(load_game_page, 0x114u) ||
        *(void **)load_game_page != game_base + RVA_LOAD_GAME_MENU_VTABLE ||
        *(unsigned int *)((uint8_t *)controller + 0x44u) != 5u) {
        SudekiMpLogFormat(
            "cleanroom_menu event=native_roster_page status=rejected "
            "reason=controller_or_load_game_gate_failed controller=%p "
            "state=%lu load_game_page=%p readable=%d actual_vtable=%p "
            "expected_vtable=%p\r\n",
            controller,
            (unsigned long)*(unsigned int *)((uint8_t *)controller + 0x44u),
            load_game_page,
            menu_memory_readable(load_game_page, 0x114u),
            menu_memory_readable(load_game_page, sizeof(void *)) ?
                *(void **)load_game_page : NULL,
            game_base + RVA_LOAD_GAME_MENU_VTABLE);
        return FALSE;
    }

    roster_native_page_controller = controller;
    roster_native_page_saved_active =
        *(void **)((uint8_t *)controller + 0xacu);
    roster_native_page_saved_backing = load_game_page;
    roster_native_page_saved_state =
        *(unsigned int *)((uint8_t *)controller + 0x44u);
    roster_native_page_saved_previous =
        *(unsigned int *)((uint8_t *)controller + 0x48u);
    roster_native_page_saved_mode =
        *(unsigned int *)((uint8_t *)controller + 0x4cu);

    ZeroMemory(roster_native_page_object + sizeof(void *),
        sizeof(roster_native_page_object) - sizeof(void *));
    *(void **)&roster_native_page_object[0] = roster_native_page_vtable;
    *(unsigned int *)((uint8_t *)controller + 0x48u) =
        roster_native_page_saved_state;
    *(unsigned int *)((uint8_t *)controller + 0x44u) = 6u;
    *(unsigned int *)((uint8_t *)controller + 0x4cu) = 2u;
    native_front_end_state_bridge(
        controller, 6u, original_front_end_state_update);

    active_page = *(void **)((uint8_t *)controller + 0xacu);
    if (active_page != load_game_page ||
        *(unsigned int *)((uint8_t *)controller + 0x44u) != 6u) {
        *(void **)((uint8_t *)controller + 0xacu) =
            roster_native_page_saved_active;
        *(void **)((uint8_t *)controller + 0xb4u) =
            roster_native_page_saved_backing;
        *(unsigned int *)((uint8_t *)controller + 0x44u) =
            roster_native_page_saved_state;
        *(unsigned int *)((uint8_t *)controller + 0x48u) =
            roster_native_page_saved_previous;
        *(unsigned int *)((uint8_t *)controller + 0x4cu) =
            roster_native_page_saved_mode;
        SudekiMpLogWrite(
            "cleanroom_menu event=native_roster_page status=rejected "
            "reason=native_state6_activation_failed\r\n");
        roster_native_page_controller = NULL;
        return FALSE;
    }
    /* The real Load Game page creates its four ICON_PORTRAIT widgets during
     * its first native update after activation.  Leave it active for exactly
     * that one update; the post-update hook then atomically moves ownership
     * to our independent input page and retries portrait attachment. */
    roster_native_page_leave_requested = FALSE;
    roster_native_page_leave_started_ms = 0u;
    roster_native_page_takeover_pending = TRUE;
    roster_native_page_state_active = TRUE;
    SudekiMpLogFormat(
        "cleanroom_menu event=native_roster_page status=active "
        "controller=%p page=%p state=6 saved_state=%lu "
        "ownership=independent_front_end_subpage\r\n",
        controller,
        roster_native_page_object,
        (unsigned long)roster_native_page_saved_state);
    return TRUE;
}

static BOOL native_roster_leave_page_state(void *controller) {
    if (!roster_native_page_state_active || controller == NULL ||
        controller != roster_native_page_controller ||
        !menu_memory_readable(controller, 0xb8u)) {
        return FALSE;
    }
    native_roster_page_release(roster_native_page_object);
    roster_native_page_takeover_pending = FALSE;
    roster_native_page_leave_requested = FALSE;
    roster_native_page_leave_started_ms = 0u;
    *(void **)((uint8_t *)controller + 0xacu) =
        roster_native_page_saved_active;
    *(void **)((uint8_t *)controller + 0xb4u) =
        roster_native_page_saved_backing;
    *(unsigned int *)((uint8_t *)controller + 0x4cu) =
        roster_native_page_saved_mode;
    *(unsigned int *)((uint8_t *)controller + 0x48u) =
        roster_native_page_saved_previous;
    *(unsigned int *)((uint8_t *)controller + 0x44u) =
        roster_native_page_saved_state;
    *(unsigned int *)&roster_native_page_object[0x2cu] = 0u;
    if (original_front_end_state_update != NULL) {
        native_front_end_state_bridge(controller,
            roster_native_page_saved_state,
            original_front_end_state_update);
    }
    roster_native_page_state_active = FALSE;
    roster_native_page_controller = NULL;
    SudekiMpLogFormat(
        "cleanroom_menu event=native_roster_page status=restored "
        "state=%lu mode=%lu\r\n",
        (unsigned long)roster_native_page_saved_state,
        (unsigned long)roster_native_page_saved_mode);
    return TRUE;
}

/* FUN_00520260 queues one animation state on a resident UI object.  The
 * object is passed in EAX, the state in EDI, and the immediate flag as one
 * callee-cleaned stack argument.  This is the same helper used by the title
 * controller for its native option-row ON/OFF/HIGHLIGHT transitions. */
__attribute__((naked, noinline, used))
static BOOL native_ui_request_state_bridge(
    void *object,
    unsigned int state,
    unsigned int immediate,
    void *target
) {
    __asm__ volatile(
        "pushl %ebp\n\t"
        "movl %esp, %ebp\n\t"
        "pushl %edi\n\t"
        "pushl 16(%ebp)\n\t"
        "movl 12(%ebp), %edi\n\t"
        "movl 8(%ebp), %eax\n\t"
        "call *20(%ebp)\n\t"
        "popl %edi\n\t"
        "popl %ebp\n\t"
        "ret\n\t"
    );
}

/* UIAnimatedMesh title rows use a small MSVC register convention that is
 * not expressible as a normal C function type: EDI is the allocated object,
 * ECX is the registered front-end resource ID, AL is the initial visibility
 * byte, and the one callee-cleaned stack byte selects the load path.
 * FUN_0049F110 uses the asynchronous preload path (1) while constructing the
 * front-end controller.  SudekiMP creates rows after that preload phase, so
 * it uses the direct active-CUIScene path (0), which completes the row's
 * render and animation setup immediately. */
__attribute__((naked, noinline, used))
static void *native_animated_title_row_construct_bridge(
    void *allocation,
    unsigned int resource_id,
    void *target
) {
    __asm__ volatile(
        "pushl %ebp\n\t"
        "movl %esp, %ebp\n\t"
        "pushl %edi\n\t"
        "movl 8(%ebp), %edi\n\t"
        "movl 12(%ebp), %ecx\n\t"
        "xorl %eax, %eax\n\t"
        "pushl $0\n\t"
        "call *16(%ebp)\n\t"
        "popl %edi\n\t"
        "popl %ebp\n\t"
        "ret\n\t"
    );
}

static void native_roster_release_animated_rows(void) {
    unsigned int index;
    AnimatedTitleRowDestructorFunction destroy;

    if (!roster_native_animated_rows_owned || game_base == NULL) {
        ZeroMemory(roster_native_animated_rows,
            sizeof(roster_native_animated_rows));
        roster_native_animated_rows_owned = FALSE;
        return;
    }
    destroy = (AnimatedTitleRowDestructorFunction)(
        game_base + RVA_ANIMATED_TITLE_ROW_DESTRUCTOR);
    for (index = 0u; index < NATIVE_ROSTER_ROW_COUNT; ++index) {
        void *row = roster_native_animated_rows[index];
        roster_native_animated_rows[index] = NULL;
        if (row == NULL ||
            !menu_memory_readable(row, NATIVE_ANIMATED_TITLE_ROW_SIZE) ||
            *(void **)row != game_base + RVA_ANIMATED_TITLE_ROW_VTABLE ||
            !menu_memory_executable((const void *)destroy)) {
            continue;
        }
        destroy(row, 1u);
    }
    roster_native_animated_rows_owned = FALSE;
    SudekiMpLogWrite(
        "cleanroom_menu event=native_roster_animated_rows status=released "
        "ownership=mod_native_ui_objects stock_title_rows=untouched\r\n");
}

static void native_roster_probe_animated_row_submodels(void);
static void roster_request_native_portraits(void *controller);
static void roster_hide_native_portrait_anchor(void *icon);
static void roster_release_native_portraits(void);

static BOOL native_roster_create_animated_rows(void) {
    static const uint8_t constructor_prefix[] = {
        0xd9, 0xee, 0x83, 0xec, 0x14, 0xc7, 0x07
    };
    static const uint8_t constructor_suffix[] = {
        0x88, 0x47, 0x11, 0x53, 0x33
    };
    static const uint8_t destructor_entry[] = {
        0x56, 0x8b, 0xf1, 0xe8, 0xf8, 0x01, 0x00, 0x00,
        0xf6, 0x44, 0x24, 0x08, 0x01
    };
    GameAllocateFunction allocate;
    uint8_t *constructor;
    uint8_t *destructor;
    unsigned int index;

    if (roster_native_animated_rows_owned) {
        return TRUE;
    }
    if (game_base == NULL) {
        return FALSE;
    }
    allocate = (GameAllocateFunction)(game_base + RVA_GAME_ALLOCATE);
    constructor = game_base + RVA_ANIMATED_TITLE_ROW_CONSTRUCTOR;
    destructor = game_base + RVA_ANIMATED_TITLE_ROW_DESTRUCTOR;
    if (!menu_memory_executable((const void *)allocate) ||
        !menu_memory_readable(constructor,
            sizeof(constructor_prefix) + sizeof(void *) +
                sizeof(constructor_suffix)) ||
        !menu_memory_executable(constructor) ||
        memcmp(constructor, constructor_prefix,
            sizeof(constructor_prefix)) != 0 ||
        *(void **)(constructor + sizeof(constructor_prefix)) !=
            game_base + RVA_ANIMATED_TITLE_ROW_VTABLE ||
        memcmp(constructor + sizeof(constructor_prefix) + sizeof(void *),
            constructor_suffix, sizeof(constructor_suffix)) != 0 ||
        !menu_memory_readable(destructor, sizeof(destructor_entry)) ||
        !menu_memory_executable(destructor) ||
        memcmp(destructor, destructor_entry,
            sizeof(destructor_entry)) != 0 ||
        !menu_memory_readable(
            game_base + RVA_ANIMATED_TITLE_ROW_VTABLE, 0x10u)) {
        SudekiMpLogWrite(
            "cleanroom_menu event=native_roster_animated_rows "
            "status=rejected reason=exact_native_contract_gate\r\n");
        return FALSE;
    }
    ZeroMemory(roster_native_animated_rows,
        sizeof(roster_native_animated_rows));
    /* Mark ownership before construction so the partial-failure path can
     * destroy every fully constructed prefix through its native destructor. */
    roster_native_animated_rows_owned = TRUE;
    for (index = 0u; index < NATIVE_ROSTER_ROW_COUNT; ++index) {
        void *allocation = allocate(NATIVE_ANIMATED_TITLE_ROW_SIZE);
        void *row;
        if (allocation == NULL) {
            native_roster_release_animated_rows();
            return FALSE;
        }
        row = native_animated_title_row_construct_bridge(
            allocation,
            NATIVE_ROSTER_FIRST_RESOURCE_ID + index,
            constructor);
        if (row != allocation ||
            !menu_memory_readable(row, NATIVE_ANIMATED_TITLE_ROW_SIZE) ||
            *(void **)row != game_base + RVA_ANIMATED_TITLE_ROW_VTABLE) {
            /* A successfully returned object owns its allocation.  Record it
             * before using the ordinary prefix cleanup.  A constructor that
             * returns a foreign pointer is a fail-closed leak rather than an
             * unsafe free of an unknown object. */
            if (row == allocation) {
                roster_native_animated_rows[index] = row;
            }
            native_roster_release_animated_rows();
            return FALSE;
        }
        roster_native_animated_rows[index] = row;
    }
    SudekiMpLogFormat(
        "cleanroom_menu event=native_roster_animated_rows status=created "
        "resources=0x63,0x64,0x65,0x66,0x67 rows=%p,%p,%p,%p,%p "
        "ownership=independent_native_uianimatedmesh\r\n",
        roster_native_animated_rows[0], roster_native_animated_rows[1],
        roster_native_animated_rows[2], roster_native_animated_rows[3],
        roster_native_animated_rows[4]);
    native_roster_probe_animated_row_submodels();
    return TRUE;
}

static unsigned int native_roster_name_hash(const char *name) {
    unsigned int hash = 0u;

    if (name == NULL) {
        return 0u;
    }
    while (*name != '\0') {
        unsigned char value = (unsigned char)*name++;
        if (value >= (unsigned char)'A' && value <= (unsigned char)'Z') {
            value = (unsigned char)(value + ('a' - 'A'));
        }
        hash = (unsigned int)value ^ hash * 0x21u;
    }
    return hash;
}

static void native_roster_probe_animated_row_submodels(void) {
    unsigned int index;

    if (!roster_native_animated_rows_owned || game_base == NULL) {
        return;
    }
    for (index = 0u; index < NATIVE_ROSTER_ROW_COUNT; ++index) {
        void *row = roster_native_animated_rows[index];
        uint8_t *node;
        void *renderer;
        void **vtable;
        char candidate_names[7u][32u];
        unsigned int candidate_hashes[7u];
        unsigned int candidate_indices[7u];
        void *candidate_objects[7u];
        unsigned int candidate;
        unsigned int component_count;
        unsigned int submodel_count;
        AnimRendererNamedIndexFunction find_index;
        AnimRendererSubmodelObjectFunction get_component;
        AnimRendererCountFunction get_component_count;
        AnimRendererCountFunction get_count;

        if (!menu_memory_readable(row, 0xc0u) ||
            *(void **)row != game_base + RVA_ANIMATED_TITLE_ROW_VTABLE) {
            SudekiMpLogFormat(
                "cleanroom_menu event=native_roster_animated_submodel_probe "
                "row=%lu status=rejected reason=row_unreadable row_object=%p\r\n",
                (unsigned long)index, row);
            continue;
        }
        node = *(uint8_t **)((uint8_t *)row + 0xbcu);
        if (!menu_memory_readable(node, 0x14u)) {
            SudekiMpLogFormat(
                "cleanroom_menu event=native_roster_animated_submodel_probe "
                "row=%lu status=rejected reason=node_unreadable node=%p\r\n",
                (unsigned long)index, node);
            continue;
        }
        renderer = *(void **)(node + 0x10u);
        if (!menu_memory_readable(renderer, sizeof(void *))) {
            SudekiMpLogFormat(
                "cleanroom_menu event=native_roster_animated_submodel_probe "
                "row=%lu status=rejected reason=renderer_unreadable "
                "node=%p renderer=%p\r\n",
                (unsigned long)index, node, renderer);
            continue;
        }
        vtable = *(void ***)renderer;
        if (vtable != (void **)(game_base +
                RVA_ANIM_OBJECT_RENDERER_VTABLE) ||
            !menu_memory_readable(vtable, 0xfcu)) {
            SudekiMpLogFormat(
                "cleanroom_menu event=native_roster_animated_submodel_probe "
                "row=%lu status=rejected reason=renderer_vtable_mismatch "
                "node=%p renderer=%p renderer_vtable=%p expected=%p\r\n",
                (unsigned long)index, node, renderer, vtable,
                game_base + RVA_ANIM_OBJECT_RENDERER_VTABLE);
            continue;
        }
        get_component_count = (AnimRendererCountFunction)
            vtable[0x18u / sizeof(void *)];
        get_component = (AnimRendererSubmodelObjectFunction)
            vtable[0x1cu / sizeof(void *)];
        find_index = (AnimRendererNamedIndexFunction)
            vtable[0x28u / sizeof(void *)];
        get_count = (AnimRendererCountFunction)
            vtable[0xf8u / sizeof(void *)];
        if (get_component_count != (AnimRendererCountFunction)(game_base +
                RVA_ANIM_RENDERER_COMPONENT_COUNT) ||
            get_component != (AnimRendererSubmodelObjectFunction)(game_base +
                RVA_ANIM_RENDERER_COMPONENT_OBJECT) ||
            find_index != (AnimRendererNamedIndexFunction)(game_base +
                RVA_ANIM_RENDERER_NAMED_COMPONENT) ||
            !menu_memory_executable((const void *)get_component_count) ||
            get_count != (AnimRendererCountFunction)(game_base +
                RVA_ANIM_RENDERER_SUBMODEL_COUNT) ||
            !menu_memory_executable((const void *)find_index) ||
            !menu_memory_executable((const void *)get_component) ||
            !menu_memory_executable((const void *)get_count)) {
            SudekiMpLogFormat(
                "cleanroom_menu event=native_roster_animated_component_probe "
                "row=%lu status=rejected reason=method_gate "
                "component_count=%p/%p component_object=%p/%p "
                "named_component=%p/%p submodel_count=%p/%p\r\n",
                (unsigned long)index,
                get_component_count,
                    game_base + RVA_ANIM_RENDERER_COMPONENT_COUNT,
                get_component,
                    game_base + RVA_ANIM_RENDERER_COMPONENT_OBJECT,
                find_index,
                    game_base + RVA_ANIM_RENDERER_NAMED_COMPONENT,
                get_count,
                    game_base + RVA_ANIM_RENDERER_SUBMODEL_COUNT);
            continue;
        }
        wsprintfA(candidate_names[0], "Option%lu", (unsigned long)index + 1u);
        wsprintfA(candidate_names[1], "Option%lu_text",
            (unsigned long)index + 1u);
        wsprintfA(candidate_names[2], "Option%lu_textSG",
            (unsigned long)index + 1u);
        wsprintfA(candidate_names[3], "Option%lu_textShape",
            (unsigned long)index + 1u);
        wsprintfA(candidate_names[4], "Option%lu_Bar",
            (unsigned long)index + 1u);
        wsprintfA(candidate_names[5], "Option%lu_Bar_Highlight",
            (unsigned long)index + 1u);
        wsprintfA(candidate_names[6], "Option%lu_Bar_Select",
            (unsigned long)index + 1u);
        component_count = get_component_count(renderer);
        for (candidate = 0u; candidate < 7u; ++candidate) {
            candidate_hashes[candidate] =
                native_roster_name_hash(candidate_names[candidate]);
            candidate_indices[candidate] = find_index(
                renderer, candidate_hashes[candidate]);
            candidate_objects[candidate] =
                candidate_indices[candidate] < component_count ?
                    get_component(renderer, candidate_indices[candidate]) :
                    NULL;
        }
        submodel_count = get_count(renderer);
        SudekiMpLogFormat(
            "cleanroom_menu event=native_roster_animated_submodel_probe "
            "row=%lu resource=0x%02lx renderer=%p renderer_vtable=%p "
            "component_count=%lu submodel_count=%lu candidates="
            "%s:0x%08lx:%lu:%p,%s:0x%08lx:%lu:%p,"
            "%s:0x%08lx:%lu:%p,%s:0x%08lx:%lu:%p,"
            "%s:0x%08lx:%lu:%p,%s:0x%08lx:%lu:%p,"
            "%s:0x%08lx:%lu:%p policy=read_only\r\n",
            (unsigned long)index,
            (unsigned long)(NATIVE_ROSTER_FIRST_RESOURCE_ID + index),
            renderer, vtable, (unsigned long)component_count,
                (unsigned long)submodel_count,
            candidate_names[0], (unsigned long)candidate_hashes[0],
                (unsigned long)candidate_indices[0], candidate_objects[0],
            candidate_names[1], (unsigned long)candidate_hashes[1],
                (unsigned long)candidate_indices[1], candidate_objects[1],
            candidate_names[2], (unsigned long)candidate_hashes[2],
                (unsigned long)candidate_indices[2], candidate_objects[2],
            candidate_names[3], (unsigned long)candidate_hashes[3],
                (unsigned long)candidate_indices[3], candidate_objects[3],
            candidate_names[4], (unsigned long)candidate_hashes[4],
                (unsigned long)candidate_indices[4], candidate_objects[4],
            candidate_names[5], (unsigned long)candidate_hashes[5],
                (unsigned long)candidate_indices[5], candidate_objects[5],
            candidate_names[6], (unsigned long)candidate_hashes[6],
                (unsigned long)candidate_indices[6], candidate_objects[6]);
    }
}

static BOOL native_roster_bind_options_rows(void *controller) {
    static const uint8_t options_rows_initialize_entry[] = {
        0x55, 0x8b, 0xec, 0x83, 0xe4, 0xf0,
        0x81, 0xec, 0x94, 0x00, 0x00, 0x00,
        0x53, 0x56, 0x8b, 0xf1
    };
    unsigned int index;
    void *options_page;
    void *row;
    uint8_t *initialize_entry;

    if (roster_native_options_rows_bound) {
        return controller == roster_native_page_controller;
    }
    if (controller == NULL || game_base == NULL ||
        !menu_memory_readable(controller,
            NATIVE_TITLE_ROW_POINTER_OFFSET +
                NATIVE_TITLE_ROW_COUNT * sizeof(void *))) {
        return FALSE;
    }
    options_page = *(void **)((uint8_t *)controller + 0xb0u);
    if (!menu_memory_readable(options_page,
            NATIVE_OPTIONS_ROW_POINTER_OFFSET +
                NATIVE_TITLE_ROW_COUNT * sizeof(void *)) ||
        *(void **)options_page != game_base + RVA_OPTIONS_MENU_VTABLE) {
        return FALSE;
    }
    /* UILayerOptionsMenu is resident from title construction, but its nine
     * UIElementBar children are lazy: the stock Options activation calls
     * FUN_0051D4D0 once, guarded by options+0x48.  Our sibling state-6 page
     * deliberately does not activate the concrete Options page, so initialize
     * that page's owned children through the same exact native function before
     * borrowing any of them.  This preserves native allocation, resource
     * registration, refcounts, and destruction ownership. */
    if (*(uint8_t *)((uint8_t *)options_page + 0x48u) == 0u) {
        initialize_entry = game_base + RVA_OPTIONS_ROWS_INITIALIZE;
        if (!menu_memory_readable(initialize_entry,
                sizeof(options_rows_initialize_entry)) ||
            !menu_memory_executable(initialize_entry) ||
            memcmp(initialize_entry, options_rows_initialize_entry,
                sizeof(options_rows_initialize_entry)) != 0) {
            SudekiMpLogWrite(
                "cleanroom_menu event=native_roster_options_rows "
                "status=rejected reason=initializer_signature_gate\r\n");
            return FALSE;
        }
        ((OptionsRowsInitializeFunction)initialize_entry)(options_page);
        SudekiMpLogFormat(
            "cleanroom_menu event=native_roster_options_rows "
            "status=initialized options=%p initializer_rva=0x%08lx\r\n",
            options_page, (unsigned long)RVA_OPTIONS_ROWS_INITIALIZE);
    }
    for (index = 0u; index < NATIVE_TITLE_ROW_COUNT; ++index) {
        int renderer_count;
        int renderer_handle = -2;
        int *renderer_handles;
        row = *(void **)((uint8_t *)options_page +
            NATIVE_OPTIONS_ROW_POINTER_OFFSET + index * sizeof(void *));
        if (!menu_memory_readable(row, 0xc0u) ||
            *(void **)row != game_base + RVA_UI_ELEMENT_BAR_VTABLE) {
            SudekiMpLogFormat(
                "cleanroom_menu event=native_roster_options_rows "
                "status=rejected reason=row_gate_failed index=%lu row=%p\r\n",
                (unsigned long)index, row);
            return FALSE;
        }
        roster_native_options_rows[index] = row;
        renderer_count = *(int *)((uint8_t *)row + 0x38u);
        renderer_handles = *(int **)((uint8_t *)row + 0x40u);
        if (renderer_count > 0 && renderer_count <= 8 &&
            menu_memory_readable(renderer_handles,
                (size_t)renderer_count * sizeof(*renderer_handles))) {
            renderer_handle = renderer_handles[0];
        }
        SudekiMpLogFormat(
            "cleanroom_menu event=native_roster_options_row_probe "
            "index=%lu row=%p desired=%lu floor=%lu applied=%lu "
            "renderer_count=%ld renderer_handles=%p first_handle=%ld\r\n",
            (unsigned long)index, row,
            (unsigned long)*(unsigned int *)((uint8_t *)row + 0x1cu),
            (unsigned long)*(unsigned int *)((uint8_t *)row + 0x20u),
            (unsigned long)*(unsigned int *)((uint8_t *)row + 0x24u),
            (long)renderer_count, renderer_handles, (long)renderer_handle);
    }
    roster_native_options_rows_bound = TRUE;
    SudekiMpLogFormat(
        "cleanroom_menu event=native_roster_options_rows status=bound "
        "controller=%p options=%p rows=%p,%p,%p,%p,%p "
        "ownership=resident_options_native_widgets\r\n",
        controller, options_page,
        roster_native_options_rows[0], roster_native_options_rows[1],
        roster_native_options_rows[2], roster_native_options_rows[3],
        roster_native_options_rows[4]);
    return TRUE;
}

static BOOL native_roster_set_options_row_state(
    void *row,
    unsigned int state
) {
    void **vtable;
    UiElementBarUpdateFunction update;

    if (row == NULL || game_base == NULL ||
        !menu_memory_readable(row, 0x5cu) ||
        *(void **)row != game_base + RVA_UI_ELEMENT_BAR_VTABLE) {
        return FALSE;
    }
    vtable = *(void ***)row;
    if (!menu_memory_readable(vtable, 0x18u)) {
        return FALSE;
    }
    update = (UiElementBarUpdateFunction)vtable[0x14u / sizeof(void *)];
    if (update != (UiElementBarUpdateFunction)(game_base + 0x0015c2d0u) ||
        !menu_memory_executable((const void *)update)) {
        return FALSE;
    }
    *(unsigned int *)((uint8_t *)row + 0x1cu) = state;
    update(row);
    return *(unsigned int *)((uint8_t *)row + 0x24u) ==
        (*(unsigned int *)((uint8_t *)row + 0x1cu) >=
            *(unsigned int *)((uint8_t *)row + 0x20u) ?
         *(unsigned int *)((uint8_t *)row + 0x1cu) :
         *(unsigned int *)((uint8_t *)row + 0x20u));
}

static void native_roster_release_options_rows(void *controller) {
    unsigned int index;

    if (!roster_native_options_rows_bound || controller == NULL ||
        game_base == NULL || !menu_memory_readable(controller,
            NATIVE_TITLE_ROW_POINTER_OFFSET +
                NATIVE_TITLE_ROW_COUNT * sizeof(void *))) {
        return;
    }
    for (index = 0u; index < NATIVE_TITLE_ROW_COUNT; ++index) {
        void *row = roster_native_options_rows[index];
        (void)native_roster_set_options_row_state(
            row, NATIVE_UI_ROW_STATE_OFF);
    }
    ZeroMemory(roster_native_options_rows,
        sizeof(roster_native_options_rows));
    roster_native_options_rows_bound = FALSE;
    SudekiMpLogWrite(
        "cleanroom_menu event=native_roster_options_rows status=released "
        "restoration=options_rows_off_title_row_array_untouched\r\n");
}

static void native_roster_start_page_transition(BOOL from_title) {
    roster_native_transition_started = GetTickCount();
    roster_native_transition_from_title = from_title;
}

static unsigned int native_roster_page_alpha(void) {
    DWORD elapsed;
    DWORD delay;
    DWORD fade_elapsed;

    if (roster_native_transition_started == 0u) {
        return 0xffu;
    }
    elapsed = GetTickCount() - roster_native_transition_started;
    delay = roster_native_transition_from_title ? 260u : 0u;
    if (elapsed <= delay) {
        return 0u;
    }
    fade_elapsed = elapsed - delay;
    if (fade_elapsed >= 220u) {
        roster_native_transition_started = 0u;
        roster_native_transition_from_title = FALSE;
        return 0xffu;
    }
    return (unsigned int)((fade_elapsed * 255u) / 220u);
}

static BOOL native_roster_submit_text(
    const char *text,
    unsigned int x,
    unsigned int y,
    unsigned int color
) {
    uint8_t native_text[0x3cu];
    void *scene;

    if (game_base == NULL || text == NULL || strlen(text) > 27u) {
        return FALSE;
    }
    scene = *(void * volatile *)(game_base + RVA_UI_SCENE_GLOBAL);
    if (scene == NULL) {
        return FALSE;
    }
    ZeroMemory(native_text, sizeof(native_text));
    native_menu_set_utf16_string(native_text, text);
    native_ui_submit_text_bridge(
        scene,
        native_text,
        NATIVE_UI_TEXT_FONT_TITLE,
        NATIVE_UI_TEXT_ALIGNMENT_TITLE_LEFT,
        x,
        y,
        color,
        game_base + RVA_UI_TEXT_SUBMIT);
    return TRUE;
}

/* These are measured title-font advances from the live title renderer, not
 * copied artwork.  A small fallback keeps future short diagnostics sensible;
 * the shipped roster labels use the exact entries below. */
static unsigned int native_roster_title_text_width(const char *text) {
    if (text == NULL) return 0u;
    if (strcmp(text, "SUDEKI TOGETHER") == 0) return 129u;
    if (strcmp(text, "Single Player") == 0) return 91u;
    if (strcmp(text, "Co-op") == 0) return 42u;
    if (strcmp(text, "SudekiMP Settings") == 0) return 124u;
    if (strcmp(text, "TALOS CO-OP SETTINGS") == 0) return 157u;
    if (strcmp(text, "ENTER SELECTS") == 0) return 108u;
    if (strcmp(text, "PLAYER 1 - CHOOSE YOUR HERO") == 0) return 213u;
    if (strcmp(text, "PLAYER 2 - CHOOSE YOUR HERO") == 0) return 213u;
    if (strcmp(text, "CONFIRM CO-OP ROSTER") == 0) return 160u;
    if (strcmp(text, "Ailish") == 0) return 36u;
    if (strcmp(text, "Tal") == 0) return 20u;
    if (strcmp(text, "Buki") == 0) return 27u;
    if (strcmp(text, "Elco") == 0) return 26u;
    return (unsigned int)strlen(text) * 8u;
}

static BOOL native_roster_submit_centered_title_text(
    const char *text,
    unsigned int center_x,
    unsigned int y,
    unsigned int color
) {
    unsigned int width = native_roster_title_text_width(text);
    unsigned int left = center_x > width / 2u ? center_x - width / 2u : 0u;

    return native_roster_submit_text(text, left, y, color);
}

static unsigned int native_roster_item_count(void) {
    if (roster_native_screen_kind == NATIVE_ROSTER_MODE) {
        return 4u;
    }
    if (roster_native_screen_kind == NATIVE_ROSTER_CONFIRM) {
        return 4u;
    }
    if (roster_native_screen_kind == NATIVE_ROSTER_PLAYER_ONE ||
        roster_native_screen_kind == NATIVE_ROSTER_PLAYER_TWO) {
        return 5u;
    }
    if (roster_native_screen_kind == NATIVE_ROSTER_SETTINGS) {
        return 5u;
    }
    return 0u;
}

static BOOL native_roster_validate_label_object(
    void *object,
    void **renderer_result,
    UiTextColorFunction *color_result
) {
    void *renderer;
    void **vtable;
    UiTextColorFunction set_color;

    if (!menu_memory_readable(object,
            NATIVE_UI_LABEL_RENDERER_OFFSET + sizeof(void *))) {
        return FALSE;
    }
    renderer = *(void **)((uint8_t *)object +
        NATIVE_UI_LABEL_RENDERER_OFFSET);
    if (!menu_memory_readable(renderer, sizeof(void *))) {
        return FALSE;
    }
    vtable = *(void ***)renderer;
    if (!menu_memory_readable(vtable,
            NATIVE_UI_TEXT_COLOR_VTABLE_OFFSET + sizeof(void *))) {
        return FALSE;
    }
    set_color = (UiTextColorFunction)
        vtable[NATIVE_UI_TEXT_COLOR_VTABLE_OFFSET / sizeof(void *)];
    if (!menu_memory_executable((const void *)set_color)) {
        return FALSE;
    }
    if (renderer_result != NULL) {
        *renderer_result = renderer;
    }
    if (color_result != NULL) {
        *color_result = set_color;
    }
    return TRUE;
}

static BOOL native_roster_hide_stock_labels(void *controller) {
    unsigned int index;
    void *renderers[NATIVE_TITLE_ROW_COUNT];
    UiTextColorFunction setters[NATIVE_TITLE_ROW_COUNT];
    const float transparent[4] = {1.0f, 1.0f, 1.0f, 0.0f};

    if (!menu_memory_readable(controller,
            NATIVE_TITLE_LABEL_POINTER_OFFSET +
                NATIVE_TITLE_ROW_COUNT * sizeof(void *))) {
        return FALSE;
    }
    for (index = 0u; index < NATIVE_TITLE_ROW_COUNT; ++index) {
        void *object = *(void **)((uint8_t *)controller +
            NATIVE_TITLE_LABEL_POINTER_OFFSET + index * sizeof(void *));
        if (!native_roster_validate_label_object(
                object, &renderers[index], &setters[index])) {
            return FALSE;
        }
        if (roster_native_label_states_valid &&
            roster_native_label_objects[index] != object) {
            return FALSE;
        }
    }
    if (!roster_native_label_states_valid) {
        for (index = 0u; index < NATIVE_TITLE_ROW_COUNT; ++index) {
            void *object = *(void **)((uint8_t *)controller +
                NATIVE_TITLE_LABEL_POINTER_OFFSET + index * sizeof(void *));
            roster_native_label_objects[index] = object;
        }
        roster_native_label_states_valid = TRUE;
    }
    for (index = 0u; index < NATIVE_TITLE_ROW_COUNT; ++index) {
        setters[index](renderers[index], 0u, transparent);
    }
    return TRUE;
}

static void native_roster_restore_stock_labels(void) {
    unsigned int index;
    const float visible[4] = {1.0f, 1.0f, 1.0f, 1.0f};

    if (!roster_native_label_states_valid) {
        return;
    }
    for (index = 0u; index < NATIVE_TITLE_ROW_COUNT; ++index) {
        void *renderer = NULL;
        UiTextColorFunction set_color = NULL;
        void *object = roster_native_label_objects[index];
        if (!native_roster_validate_label_object(
                object, &renderer, &set_color)) {
            continue;
        }
        set_color(renderer, 0u, visible);
    }
    ZeroMemory(roster_native_label_objects,
        sizeof(roster_native_label_objects));
    roster_native_label_states_valid = FALSE;
    roster_native_rows_active = FALSE;
}

static BOOL native_roster_queue_rows(
    void *controller,
    unsigned int count,
    unsigned int selection
) {
    unsigned int index;
    uint8_t *request_state;
    void *rows[NATIVE_TITLE_ROW_COUNT];

    if (game_base == NULL || count == 0u || count > NATIVE_TITLE_ROW_COUNT ||
        selection >= count ||
        !menu_memory_readable(controller,
            NATIVE_TITLE_ROW_POINTER_OFFSET +
                NATIVE_TITLE_ROW_COUNT * sizeof(void *))) {
        return FALSE;
    }
    if (roster_native_options_rows_bound) {
        for (index = 0u; index < NATIVE_TITLE_ROW_COUNT; ++index) {
            unsigned int state = index >= count ? NATIVE_UI_ROW_STATE_OFF :
                (index == selection ? NATIVE_UI_ROW_STATE_HIGHLIGHT :
                    NATIVE_UI_ROW_STATE_IDLE);
            if (!native_roster_set_options_row_state(
                    roster_native_options_rows[index], state)) {
                return FALSE;
            }
        }
        return TRUE;
    }
    if (roster_native_animated_rows_owned) {
        request_state = game_base + RVA_UI_STATE_REQUEST;
        if (!menu_memory_executable(request_state) ||
            count > NATIVE_ROSTER_ROW_COUNT) {
            return FALSE;
        }
        for (index = 0u; index < NATIVE_ROSTER_ROW_COUNT; ++index) {
            /* These private objects replace the resident title rows only for
             * the duration of the render call.  Keep their baked localized
             * meshes OFF; SudekiMP draws independent, label-free capsules at
             * the UI-scene boundary and leaves native queued text on top. */
            unsigned int state = NATIVE_UI_ROW_STATE_OFF;
            void *row = roster_native_animated_rows[index];
            if (!menu_memory_readable(row,
                    NATIVE_ANIMATED_TITLE_ROW_SIZE) ||
                *(void **)row !=
                    game_base + RVA_ANIMATED_TITLE_ROW_VTABLE) {
                return FALSE;
            }
            *(unsigned int *)((uint8_t *)row +
                NATIVE_UI_ROW_QUEUE_READ_OFFSET) =
                    *(unsigned int *)((uint8_t *)row +
                        NATIVE_UI_ROW_QUEUE_WRITE_OFFSET);
            if (!native_ui_request_state_bridge(
                    row, state, 1u, request_state)) {
                return FALSE;
            }
        }
        /* Unlike the original title-row reuse experiments, these rows do not
         * borrow controller+0x70 or its +0x17D4/+0x17D8 selection records.
         * The stock title page therefore cannot follow our navigation. */
        return TRUE;
    }
    /* Never fall back to controller+0x70..+0x80.  Those are the resident
     * Continue/New Game/... presentations, and borrowing them would make two
     * logical pages share one visual selection.  Failed private rows degrade
     * to the independent page's native-font text only. */
    (void)controller;
    (void)index;
    (void)request_state;
    (void)rows;
    return FALSE;
}

static void native_roster_restore_vanilla_items(void *controller) {
    if (controller == NULL || front_end_menu_builder == NULL) {
        return;
    }
    native_roster_restore_stock_labels();
    /* This is the same rebuild used by native title navigation.  It restores
     * the shipped button resources, localized labels, actions and colors. */
    front_end_menu_builder(controller);
    if (front_end_selection_refresh != NULL) {
        front_end_selection_refresh(controller);
    }
}

static void native_roster_submit_page(void) {
    static const char *const mode_labels[] = {
        "Single Player", "Co-op", "SudekiMP Settings", "Back"
    };
    static const char *const actor_labels[] = {"Ailish", "Tal", "Buki", "Elco"};
    /* The card texture uses its own centered 640-wide overlay.  Font-1 title
     * text is submitted in the title renderer's logical coordinates, so these
     * are the measured centers of those four on-screen cards in that space. */
    static const unsigned int actor_title_card_centers[] = {
        218u, 286u, 353u, 421u
    };
    char confirm_player_one[28];
    char confirm_player_two[28];
    char talos_tuning[28];
    char talos_health[28];
    char talos_stagger[28];
    char talos_window[28];
    const char *actor_back_label = "Back";
    const char *const *labels = NULL;
    const char *heading = NULL;
    unsigned int count = 0u;
    unsigned int selection = 0u;
    unsigned int index;
    unsigned int alpha;
    unsigned int prompt_y;
    const char *confirm_labels[4];
    const char *settings_labels[5];

    if (!roster_native_screen || roster_pending_controller == NULL) {
        return;
    }
    selection = roster_native_selection;
    if (roster_native_screen_kind == NATIVE_ROSTER_MODE) {
        heading = "SUDEKI TOGETHER";
        labels = mode_labels;
        count = 4u;
    }
    else if (roster_native_screen_kind == NATIVE_ROSTER_PLAYER_ONE) {
        heading = "PLAYER 1 - CHOOSE YOUR HERO";
        labels = actor_labels;
        count = 5u;
    }
    else if (roster_native_screen_kind == NATIVE_ROSTER_PLAYER_TWO) {
        heading = "PLAYER 2 - CHOOSE YOUR HERO";
        labels = actor_labels;
        count = 5u;
    }
    else if (roster_native_screen_kind == NATIVE_ROSTER_CONFIRM) {
        wsprintfA(confirm_player_one, "Player 1: %s",
            roster_actor_label(roster_player_one));
        wsprintfA(confirm_player_two, "Player 2: %s",
            roster_actor_label(roster_player_two));
        confirm_labels[0] = confirm_player_one;
        confirm_labels[1] = confirm_player_two;
        confirm_labels[2] = "Lock In";
        confirm_labels[3] = "Back";
        heading = "CONFIRM CO-OP ROSTER";
        labels = confirm_labels;
        count = 4u;
    }
    else if (roster_native_screen_kind == NATIVE_ROSTER_SETTINGS) {
        wsprintfA(talos_tuning, "Talos Tuning: %s",
            roster_talos_tuning_enabled ? "ON" : "OFF");
        wsprintfA(talos_health, "Health Scale: %ux",
            roster_talos_health_scale);
        wsprintfA(talos_stagger, "Armor Hits: %u",
            roster_talos_stagger_limit);
        wsprintfA(talos_window, "Armor Window: %us",
            roster_talos_stagger_window);
        settings_labels[0] = talos_tuning;
        settings_labels[1] = talos_health;
        settings_labels[2] = talos_stagger;
        settings_labels[3] = talos_window;
        settings_labels[4] = "Back";
        heading = "TALOS CO-OP SETTINGS";
        labels = settings_labels;
        count = 5u;
    }
    if (labels == NULL || selection >= count) {
        return;
    }
    alpha = native_roster_page_alpha();
    if (alpha == 0u) {
        return;
    }
    prompt_y = (roster_native_screen_kind == NATIVE_ROSTER_PLAYER_ONE ||
        roster_native_screen_kind == NATIVE_ROSTER_PLAYER_TWO) ? 458u :
        NATIVE_ROSTER_PROMPT_Y;
    (void)native_roster_submit_centered_title_text(
        heading,
        NATIVE_ROSTER_TITLE_CONTENT_CENTER_X,
        NATIVE_ROSTER_HEADING_Y,
        0xf0dc9600u | alpha);
    if (roster_native_screen_kind == NATIVE_ROSTER_PLAYER_ONE ||
        roster_native_screen_kind == NATIVE_ROSTER_PLAYER_TWO) {
        for (index = 0u; index < 4u; ++index) {
            (void)native_roster_submit_centered_title_text(
                labels[index],
                actor_title_card_centers[index],
                403u,
                (index == selection ? 0xffffff00u : 0x9a9a9a00u) | alpha);
        }
        (void)native_roster_submit_centered_title_text(
            actor_back_label,
            NATIVE_ROSTER_TITLE_CONTENT_CENTER_X,
            430u,
            (selection == 4u ? 0xffffff00u : 0x9a9a9a00u) | alpha);
    }
    else {
        for (index = 0u; index < count; ++index) {
            (void)native_roster_submit_centered_title_text(
                labels[index],
                NATIVE_ROSTER_TITLE_CONTENT_CENTER_X,
                NATIVE_ROSTER_FIRST_ROW_Y + index * 19u,
                (index == selection ? 0xffffff00u : 0x9a9a9a00u) | alpha);
        }
    }
    (void)native_roster_submit_centered_title_text(
        "ENTER SELECTS",
        NATIVE_ROSTER_TITLE_CONTENT_CENTER_X,
        prompt_y,
        0xb8b8b800u | alpha);
}

static void native_roster_refresh_screen(void *controller) {
    unsigned int count = native_roster_item_count();

    roster_native_rows_active = FALSE;
    roster_button_texture_dirty = TRUE;
    if (roster_native_screen_kind == NATIVE_ROSTER_PLAYER_ONE ||
        roster_native_screen_kind == NATIVE_ROSTER_PLAYER_TWO) {
        /* Loading belongs to the title-update transition, not the D3D draw
         * callback.  Sudeki's cache may schedule I/O and must retain its own
         * normal update/worker ordering. */
        roster_request_native_portraits(controller);
    }
    else if (roster_native_portraits_requested) {
        roster_release_native_portraits();
    }
    if (controller == NULL || count == 0u ||
        roster_native_selection >= count ||
        !native_roster_create_animated_rows() ||
        !native_roster_hide_stock_labels(controller) ||
        !native_roster_queue_rows(
            controller, count, roster_native_selection)) {
        SudekiMpLogWrite(
            "cleanroom_menu event=native_roster_native_rows "
            "status=rejected reason=private_title_row_or_label_gate_failed "
            "fallback=native_font_text_only\r\n");
        return;
    }
    roster_native_rows_active = TRUE;
    SudekiMpLogFormat(
        "cleanroom_menu event=native_roster_native_rows status=active "
        "page=%lu selection=%lu count=%lu "
        "presentation=runtime_recreated_sudeki_capsules "
        "labels=stock_presentations_hidden_custom_native_font "
        "assets=none_copyrighted_runtime_generated\r\n",
        (unsigned long)roster_native_screen_kind,
        (unsigned long)roster_native_selection,
        (unsigned long)count);
}

static const char *native_roster_selected_action(void) {
    static const char *const mode_actions[] = {
        "SudekiMPSinglePlayer", "SudekiMPCoop", "SudekiMPSettings",
        "SudekiMPRosterBack"
    };
    static const char *const p1_actions[] = {
        "SudekiMPP1Ailish", "SudekiMPP1Tal", "SudekiMPP1Buki",
        "SudekiMPP1Elco", "SudekiMPRosterBack"
    };
    static const char *const p2_actions[] = {
        "SudekiMPP2Ailish", "SudekiMPP2Tal", "SudekiMPP2Buki",
        "SudekiMPP2Elco", "SudekiMPRosterBack"
    };
    static const char *const confirm_actions[] = {
        "SudekiMPRosterP1", "SudekiMPRosterP2", "SudekiMPRosterLock",
        "SudekiMPRosterBack"
    };
    static const char *const settings_actions[] = {
        "SudekiMPTalosToggle", "SudekiMPTalosHealth",
        "SudekiMPTalosStagger", "SudekiMPTalosWindow",
        "SudekiMPRosterBack"
    };

    if (roster_native_screen_kind == NATIVE_ROSTER_MODE &&
        roster_native_selection < 4u) {
        return mode_actions[roster_native_selection];
    }
    if (roster_native_screen_kind == NATIVE_ROSTER_PLAYER_ONE &&
        roster_native_selection < 5u) {
        return p1_actions[roster_native_selection];
    }
    if (roster_native_screen_kind == NATIVE_ROSTER_PLAYER_TWO &&
        roster_native_selection < 5u) {
        return p2_actions[roster_native_selection];
    }
    if (roster_native_screen_kind == NATIVE_ROSTER_CONFIRM &&
        roster_native_selection < 4u) {
        return confirm_actions[roster_native_selection];
    }
    if (roster_native_screen_kind == NATIVE_ROSTER_SETTINGS &&
        roster_native_selection < 5u) {
        return settings_actions[roster_native_selection];
    }
    return NULL;
}

static BOOL native_roster_restore_original_menu(void *controller);

static BOOL native_roster_back_to_title(void *controller) {
    if (controller == NULL ||
        !native_roster_restore_original_menu(controller)) {
        SudekiMpLogWrite(
            "cleanroom_menu event=native_roster status=rejected "
            "reason=back_to_title_restore_failed\r\n");
        return FALSE;
    }
    native_roster_release_animated_rows();
    native_roster_release_options_rows(controller);
    if (!native_roster_leave_page_state(controller)) {
        SudekiMpLogWrite(
            "cleanroom_menu event=native_roster status=rejected "
            "reason=back_to_title_page_restore_failed\r\n");
        return FALSE;
    }
    roster_native_screen = FALSE;
    roster_native_screen_kind = NATIVE_ROSTER_NONE;
    roster_native_selection = 0u;
    roster_waiting_new_game = FALSE;
    roster_pending_controller = NULL;
    native_roster_restore_vanilla_items(controller);
    /* Back means cancel the provisional New Game edit, not disable the
     * persisted co-op session.  Re-read and republish the sidecar so loading
     * an existing save in the same process still performs its roster handoff. */
    roster_load_persistence();
    SudekiMpLogWrite(
        "cleanroom_menu event=native_roster state=back_to_title "
        "reason=user_selection contract=restored_from_sidecar\r\n");
    return TRUE;
}

static void native_roster_rebuild_from_native_menu(void *controller) {
    /* The title front-end has its own controller tick; the gameplay
     * controller hook is not guaranteed to run while the title screen is
     * visible.  Populate the already-allocated native records immediately,
     * then let the normal tick repeat the write if needed. */
    if (controller != NULL) {
        native_roster_refresh_screen(controller);
        roster_native_screen_dirty = FALSE;
        /* One physical confirm can remain queued while the title animates to
         * the replacement page.  A time debounce is insufficient because the
         * replay may arrive after the visual transition.  Each page must see
         * the confirm controls released before it can accept a fresh press. */
        roster_confirm_input_armed = FALSE;
    }
    else {
        roster_native_screen_dirty = TRUE;
    }
}

static const char *front_end_selected_action(void *controller) {
    unsigned int index;
    uint8_t *item;
    uint32_t flags;

    if (controller == NULL) {
        return NULL;
    }
    index = *(unsigned int *)((uint8_t *)controller + 0x17d4u);
    if (index >= 32u) {
        return NULL;
    }
    item = (uint8_t *)controller + 0xfd0u + index * 0x20u;
    flags = *(uint32_t *)item;
    if ((flags & 0x80000000u) == 0u) {
        return *(const char **)(item + 4u);
    }
    return (const char *)(item + 4u);
}

static void trace_native_options_page(
    void *controller,
    const char *edge
) {
    static void *last_controller;
    static void *last_active_page;
    static char last_edge[8];
    void *active_page;
    void *options_page;
    void *alternate_page;
    uintptr_t base;
    uintptr_t active_vtable = 0u;
    uintptr_t options_vtable = 0u;
    uintptr_t alternate_vtable = 0u;

    if (controller == NULL || edge == NULL || game_base == NULL ||
        !menu_memory_readable(controller, 0xb8u)) {
        return;
    }
    active_page = *(void **)((uint8_t *)controller + 0xacu);
    options_page = *(void **)((uint8_t *)controller + 0xb0u);
    alternate_page = *(void **)((uint8_t *)controller + 0xb4u);
    if (controller == last_controller && active_page == last_active_page &&
        strcmp(edge, last_edge) == 0) {
        return;
    }
    if (menu_memory_readable(active_page, sizeof(void *))) {
        active_vtable = (uintptr_t)*(void **)active_page;
    }
    if (menu_memory_readable(options_page, sizeof(void *))) {
        options_vtable = (uintptr_t)*(void **)options_page;
    }
    if (menu_memory_readable(alternate_page, sizeof(void *))) {
        alternate_vtable = (uintptr_t)*(void **)alternate_page;
    }
    base = (uintptr_t)game_base;
    SudekiMpLogFormat(
        "cleanroom_menu event=native_options_page edge=%s controller=%p "
        "state=%lu previous=%lu mode=%lu active=%p active_vtable_rva=0x%08lx "
        "options=%p options_vtable_rva=0x%08lx alternate=%p "
        "alternate_vtable_rva=0x%08lx\r\n",
        edge,
        controller,
        (unsigned long)*(unsigned int *)((uint8_t *)controller + 0x44u),
        (unsigned long)*(unsigned int *)((uint8_t *)controller + 0x48u),
        (unsigned long)*(unsigned int *)((uint8_t *)controller + 0x4cu),
        active_page,
        active_vtable >= base ? (unsigned long)(active_vtable - base) : ~0ul,
        options_page,
        options_vtable >= base ? (unsigned long)(options_vtable - base) : ~0ul,
        alternate_page,
        alternate_vtable >= base ?
            (unsigned long)(alternate_vtable - base) : ~0ul);
    last_controller = controller;
    last_active_page = active_page;
    lstrcpynA(last_edge, edge, sizeof(last_edge));
}

static BOOL native_roster_snapshot_original_menu(void *controller) {
    unsigned int count;
    unsigned int selection;
    unsigned int index;

    if (controller == NULL) {
        return FALSE;
    }
    count = *(unsigned int *)((uint8_t *)controller + 0x17d8u);
    selection = *(unsigned int *)((uint8_t *)controller + 0x17d4u);
    if (count == 0u || count > 8u || selection >= count) {
        return FALSE;
    }
    for (index = 0u; index < count; ++index) {
        memcpy(roster_original_menu_labels[index],
            (uint8_t *)controller + 0xd0u + index * 0x3cu, 0x3cu);
        memcpy(roster_original_menu_actions[index],
            (uint8_t *)controller + 0xfd0u + index * 0x20u, 0x20u);
    }
    roster_original_menu_count = count;
    roster_original_menu_selection = selection;
    roster_original_menu_valid = TRUE;
    return TRUE;
}

static BOOL native_roster_restore_original_menu(void *controller) {
    unsigned int index;

    if (!roster_original_menu_valid || controller == NULL ||
        roster_original_menu_count == 0u ||
        roster_original_menu_count > 8u ||
        roster_original_menu_selection >= roster_original_menu_count) {
        return FALSE;
    }
    for (index = 0u; index < roster_original_menu_count; ++index) {
        memcpy((uint8_t *)controller + 0xd0u + index * 0x3cu,
            roster_original_menu_labels[index], 0x3cu);
        memcpy((uint8_t *)controller + 0xfd0u + index * 0x20u,
            roster_original_menu_actions[index], 0x20u);
    }
    *(unsigned int *)((uint8_t *)controller + 0x17d8u) =
        roster_original_menu_count;
    *(unsigned int *)((uint8_t *)controller + 0x17d4u) =
        roster_original_menu_selection;
    return TRUE;
}

static BOOL native_front_end_activation(
    unsigned int phase,
    unsigned int event,
    unsigned int argument
) {
    (void)argument;
    /* State 5 in FUN_004A0360 activates the selected native action on the
     * button-down phase.  Event 0 is keyboard/controller confirm and event 2
     * is the equivalent pointer activation.  The later phase-6/event-7 edge
     * only moves/finalizes title focus; treating it as activation caused the
     * real phase-5 confirm to fall through into Sudeki's default state 10. */
    return phase == 5u && (event == 0u || event == 2u);
}

static BOOL native_roster_confirm_input_down(unsigned int event) {
    /* Sudeki dispatches event 0 for keyboard/controller confirm and event 2
     * for pointer activation.  GetAsyncKeyState lets us distinguish a fresh
     * physical press from a stale title-controller event queued by the page
     * transition.  VK_GAMEPAD_A is 0xC3 on Windows 10+; spelling the value
     * directly keeps the MinGW build compatible with older SDK headers. */
    if (event == 2u) {
        return (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
    }
    return (GetAsyncKeyState(VK_RETURN) & 0x8000) != 0 ||
        (GetAsyncKeyState(VK_SPACE) & 0x8000) != 0 ||
        (GetAsyncKeyState(0xc3) & 0x8000) != 0;
}

static void native_roster_arm_confirm_after_release(void) {
    if (!roster_confirm_input_armed &&
        !native_roster_confirm_input_down(0u) &&
        !native_roster_confirm_input_down(2u)) {
        roster_confirm_input_armed = TRUE;
        SudekiMpLogWrite(
            "cleanroom_menu event=native_roster state=confirm_armed "
            "reason=physical_release\r\n");
    }
}

static BOOL native_roster_navigation(
    void *controller,
    unsigned int phase,
    unsigned int event
) {
    unsigned int count;
    unsigned int selection;

    if (controller == NULL || phase != 5u ||
        (event != 6u && event != 7u)) {
        return FALSE;
    }
    count = roster_native_screen_kind == NATIVE_ROSTER_MODE ? 4u :
        (roster_native_screen_kind == NATIVE_ROSTER_CONFIRM ? 4u : 5u);
    selection = roster_native_selection;
    if (count == 0u || count > NATIVE_ROSTER_ROW_COUNT || selection >= count) {
        return FALSE;
    }
    if (event == 6u) {
        selection = selection == 0u ? count - 1u : selection - 1u;
    }
    else {
        selection = selection + 1u;
        if (selection >= count) {
            selection = 0u;
        }
    }
    /* Player 2 can see Player 1's choice, but cannot focus or confirm it.
     * Treat that card as a disabled item instead of accepting a press and
     * silently rejecting it later. */
    while (roster_native_screen_kind == NATIVE_ROSTER_PLAYER_TWO &&
           selection < MENU_ACTOR_COUNT &&
           (unsigned int)roster_display_actor(selection) ==
               roster_player_one) {
        if (event == 6u) {
            selection = selection == 0u ? count - 1u : selection - 1u;
        }
        else {
            selection = selection + 1u;
            if (selection >= count) {
                selection = 0u;
            }
        }
    }
    roster_native_selection = selection;
    native_roster_refresh_screen(controller);
    SudekiMpLogFormat(
        "cleanroom_menu event=native_roster state=navigate selection=%lu "
        "count=%lu\r\n",
        (unsigned long)selection,
        (unsigned long)count);
    return TRUE;
}

static void log_front_end_update_state(
    void *controller,
    const char *edge,
    unsigned int state,
    unsigned int previous_state,
    unsigned int selected,
    unsigned int count,
    const char *action
) {
    static void *last_controller;
    static unsigned int last_state = ~0u;
    static unsigned int last_previous_state = ~0u;
    static unsigned int last_selected = ~0u;
    static unsigned int last_count = ~0u;
    static char last_action[64];
    static char last_edge[8];
    const char *safe_action = action == NULL ? "<none>" : action;

    if (controller == last_controller && state == last_state &&
        previous_state == last_previous_state && selected == last_selected &&
        count == last_count && strcmp(last_action, safe_action) == 0 &&
        strcmp(last_edge, edge) == 0) {
        return;
    }
    last_controller = controller;
    last_state = state;
    last_previous_state = previous_state;
    last_selected = selected;
    last_count = count;
    strncpy(last_action, safe_action, sizeof(last_action) - 1u);
    last_action[sizeof(last_action) - 1u] = '\0';
    strncpy(last_edge, edge, sizeof(last_edge) - 1u);
    last_edge[sizeof(last_edge) - 1u] = '\0';
    SudekiMpLogFormat(
        "cleanroom_menu event=native_front_end_update edge=%s controller=%p "
        "state=%lu previous_state=%lu selected=%lu count=%lu action=%s\r\n",
        edge,
        controller,
        (unsigned long)state,
        (unsigned long)previous_state,
        (unsigned long)selected,
        (unsigned long)count,
        safe_action);
}

static void __attribute__((thiscall)) cleanroom_front_end_update(
    void *controller,
    float delta_seconds
) {
    unsigned int state;
    unsigned int previous_state;
    unsigned int selected;
    unsigned int count;
    const char *action = NULL;

    if (controller != NULL) {
        state = *(unsigned int *)((uint8_t *)controller + 0x44u);
        previous_state = *(unsigned int *)((uint8_t *)controller + 0x48u);
        selected = *(unsigned int *)((uint8_t *)controller + 0x17d4u);
        count = *(unsigned int *)((uint8_t *)controller + 0x17d8u);
        if (state == 5u && count > 0u && count <= 32u && selected < count) {
            action = front_end_selected_action(controller);
        }
        log_front_end_update_state(controller, "before", state,
            previous_state, selected, count, action);
    }

    if (original_front_end_update != NULL) {
        original_front_end_update(controller, delta_seconds);
    }

    if (controller != NULL) {
        state = *(unsigned int *)((uint8_t *)controller + 0x44u);
        previous_state = *(unsigned int *)((uint8_t *)controller + 0x48u);
        selected = *(unsigned int *)((uint8_t *)controller + 0x17d4u);
        count = *(unsigned int *)((uint8_t *)controller + 0x17d8u);
        action = NULL;
        if (state == 5u && count > 0u && count <= 32u && selected < count) {
            action = front_end_selected_action(controller);
        }
        log_front_end_update_state(controller, "after", state,
            previous_state, selected, count, action);
    }
}

static unsigned int __attribute__((thiscall)) cleanroom_pc_front_end_action_trace(
    void *controller,
    unsigned int phase,
    unsigned int event,
    unsigned int argument
) {
    static unsigned int last_phase = ~0u;
    static unsigned int last_event = ~0u;
    static unsigned int last_mode = ~0u;
    unsigned int mode = controller == NULL ? ~0u :
        *(unsigned int *)((uint8_t *)controller + 0x44u);
    unsigned int stage = controller == NULL ? ~0u :
        *(unsigned int *)((uint8_t *)controller + 0x48u);
    unsigned int selection = controller == NULL ? ~0u :
        *(unsigned int *)((uint8_t *)controller + 0x17d4u);
    unsigned int count = controller == NULL ? 0u :
        *(unsigned int *)((uint8_t *)controller + 0x17d8u);
    const char *action = NULL;

    if (controller != NULL && game_base != NULL &&
        *(void **)controller == (void *)(game_base + RVA_FRONT_END_VTABLE) &&
        count > 0u && count <= 32u && selection < count) {
        action = front_end_selected_action(controller);
    }

    if (phase != last_phase || event != last_event || mode != last_mode) {
        SudekiMpLogFormat(
            "cleanroom_menu event=native_front_end_action controller=%p "
            "phase=%lu event_code=%lu argument=%lu stage=%lu mode=%lu "
            "selection=%lu count=%lu action=%s\r\n",
            controller,
            (unsigned long)phase,
            (unsigned long)event,
            (unsigned long)argument,
            (unsigned long)stage,
            (unsigned long)mode,
            (unsigned long)selection,
            (unsigned long)count,
            action == NULL ? "<none>" : action);
        last_phase = phase;
        last_event = event;
        last_mode = mode;
    }
    return original_front_end_action == NULL ? 0u :
        original_front_end_action(controller, phase, event, argument);
}

static void log_pc_front_end_update(void *controller, const char *edge) {
    static void *last_controller;
    static unsigned int last_stage = ~0u;
    static unsigned int last_mode = ~0u;
    static unsigned int last_selection = ~0u;
    static unsigned int last_flags = ~0u;
    static char last_edge[8];
    unsigned int stage;
    unsigned int mode;
    unsigned int selection;
    unsigned int flags;

    if (controller == NULL) {
        return;
    }
    stage = *(unsigned int *)((uint8_t *)controller + 0x2cu);
    mode = *(unsigned int *)((uint8_t *)controller + 0x4cu);
    selection = *(unsigned int *)((uint8_t *)controller + 0x6a4u);
    flags = *(unsigned char *)((uint8_t *)controller + 0x6b8u);
    if (controller == last_controller && stage == last_stage &&
        mode == last_mode && selection == last_selection &&
        flags == last_flags) {
        return;
    }
    last_controller = controller;
    last_stage = stage;
    last_mode = mode;
    last_selection = selection;
    last_flags = flags;
    strncpy(last_edge, edge, sizeof(last_edge) - 1u);
    last_edge[sizeof(last_edge) - 1u] = '\0';
    SudekiMpLogFormat(
        "cleanroom_menu event=pc_front_end_update edge=%s controller=%p "
        "stage=%lu mode=%lu selection=%lu flags=0x%02lx\r\n",
        edge,
        controller,
        (unsigned long)stage,
        (unsigned long)mode,
        (unsigned long)selection,
        (unsigned long)flags);
}

static void __attribute__((thiscall)) cleanroom_pc_front_end_update_trace(
    void *controller,
    float delta_seconds
) {
    void *title_controller;

    log_pc_front_end_update(controller, "before");
    if (original_front_end_update != NULL) {
        original_front_end_update(controller, delta_seconds);
    }
    title_controller = roster_native_page_controller;
    if (roster_native_page_takeover_pending && roster_native_screen &&
        controller != NULL && title_controller != NULL &&
        menu_memory_readable(title_controller, 0xb8u) &&
        *(void **)((uint8_t *)title_controller + 0xacu) ==
            roster_native_page_saved_backing) {
        /* State 6 allocates the Load Game page, but its named scene anchors
         * are not guaranteed to exist until the page has completed a native
         * update/render cycle.  Probe with the exact CycleIcon bind path and
         * retain the input-shielded backing page until all four anchors are
         * genuinely available.  This is an evidence gate, not a frame-count
         * guess: a slow Wine frame or asynchronous UI load simply retries on
         * the next PC front-end update. */
        roster_request_native_portraits(title_controller);
        if (!roster_native_portraits_requested) {
            log_pc_front_end_update(controller, "after");
            return;
        }
        {
            void *load_page = roster_native_page_saved_backing;
            void **load_vtable = *(void ***)load_page;
            if (!menu_memory_readable(load_vtable, 0x48u) ||
                load_vtable[0x44u / sizeof(void *)] !=
                    game_base + RVA_LOAD_GAME_PAGE_LEAVE ||
                !menu_memory_executable(
                    load_vtable[0x44u / sizeof(void *)])) {
                log_pc_front_end_update(controller, "after");
                return;
            }
            if (!roster_native_page_leave_requested) {
                ((void (__attribute__((thiscall)) *)(void *))
                    load_vtable[0x44u / sizeof(void *)])(load_page);
                roster_native_page_leave_requested = TRUE;
                roster_native_page_leave_started_ms = GetTickCount();
                SudekiMpLogWrite(
                    "cleanroom_menu event=native_roster_page "
                    "phase=backing_leave status=requested "
                    "policy=native_transition_before_takeover\r\n");
                log_pc_front_end_update(controller, "after");
                return;
            }
            /* The Load Game page's vtable+0x34 is an input/readiness query,
             * not a transition-complete predicate.  Keep the native page
             * updating for its authored exit interval instead of treating
             * that unrelated boolean as permission to tear ownership away
             * after one frame. */
            if ((DWORD)(GetTickCount() -
                    roster_native_page_leave_started_ms) < 700u) {
                log_pc_front_end_update(controller, "after");
                return;
            }
            /* Complete the authored OFF state on the page object before it
             * stops receiving native front-end updates.  The ordinary leave
             * method only queues the transition; replacing controller+0xAC
             * immediately afterward otherwise leaves the Load Game scene
             * resident and visibly frozen beneath the roster. */
            if (!menu_memory_executable(game_base + RVA_UI_STATE_REQUEST) ||
                !native_ui_request_state_bridge(
                    load_page, NATIVE_UI_ROW_STATE_OFF, 1u,
                    game_base + RVA_UI_STATE_REQUEST)) {
                SudekiMpLogWrite(
                    "cleanroom_menu event=native_roster_page "
                    "phase=backing_leave status=pending "
                    "reason=immediate_off_state_rejected\r\n");
                log_pc_front_end_update(controller, "after");
                return;
            }
            SudekiMpLogWrite(
                "cleanroom_menu event=native_roster_page "
                "phase=backing_leave status=completed "
                "policy=native_immediate_off_before_takeover\r\n");
        }
        *(unsigned int *)&roster_native_page_object[0x2cu] = 1u;
        *(void **)((uint8_t *)title_controller + 0xacu) =
            roster_native_page_object;
        roster_native_page_takeover_pending = FALSE;
        roster_native_page_leave_requested = FALSE;
        roster_native_page_leave_started_ms = 0u;
        native_roster_page_activate(roster_native_page_object);
        /* The Load Game page remains resident solely as the owner of the four
         * decoded D3D textures.  Its original anchors stay hidden; the card
         * renderer borrows the GPU textures at their new destinations. */
        {
            unsigned int actor;

            for (actor = 0u; actor < MENU_ACTOR_COUNT; ++actor) {
                void *icon = roster_native_portrait_icons[actor];

                roster_hide_native_portrait_anchor(icon);
            }
        }
        native_roster_refresh_screen(title_controller);
        SudekiMpLogFormat(
            "cleanroom_menu event=native_roster_page phase=post_update_takeover "
            "status=active title_controller=%p renderer_controller=%p "
            "portrait_anchors=confirmed\r\n",
            title_controller,
            controller);
    }
    /* The native title update refreshes the five localized stock labels
     * after navigation and transition work.  Hiding them before this call is
     * therefore not stable: Continue/New Game/... can be made visible again
     * while SudekiMP's independent SFE_OPTION rows remain selected.  Apply
     * the presentation-only alpha override after the native update and
     * before the later front-end render submits the text. */
    if (roster_native_screen && controller != NULL && game_base != NULL &&
        *(void **)controller ==
            (void *)(game_base + RVA_PC_FRONT_END_VTABLE)) {
        (void)native_roster_hide_stock_labels(controller);
    }
    log_pc_front_end_update(controller, "after");
}

static void __attribute__((thiscall)) cleanroom_pc_front_end_render(
    void *controller
) {
    void *saved_title_rows[NATIVE_TITLE_ROW_COUNT];
    void *row_controller = roster_native_page_controller;
    BOOL animated_rows_staged = FALSE;
    unsigned int index;

    if (roster_native_screen && controller != NULL && game_base != NULL &&
        *(void **)controller == (void *)(game_base + RVA_PC_FRONT_END_VTABLE)) {
        /* FUN_004A1950 may restore the five stock title-label colors after a
         * page transition.  Reapply zero alpha at the last controller-owned
         * boundary before the resident UI objects submit their frame. */
        (void)native_roster_hide_stock_labels(controller);
        if (roster_native_animated_rows_owned &&
            row_controller != NULL &&
            menu_memory_readable(row_controller,
                NATIVE_TITLE_ROW_POINTER_OFFSET +
                    NATIVE_TITLE_ROW_COUNT * sizeof(void *))) {
            animated_rows_staged = TRUE;
            for (index = 0u; index < NATIVE_TITLE_ROW_COUNT; ++index) {
                void *row = roster_native_animated_rows[index];
                if (!menu_memory_readable(
                        row, NATIVE_ANIMATED_TITLE_ROW_SIZE) ||
                    *(void **)row !=
                        game_base + RVA_ANIMATED_TITLE_ROW_VTABLE) {
                    animated_rows_staged = FALSE;
                    break;
                }
                saved_title_rows[index] = *(void **)((uint8_t *)row_controller +
                    NATIVE_TITLE_ROW_POINTER_OFFSET +
                        index * sizeof(void *));
            }
            if (animated_rows_staged) {
                for (index = 0u; index < NATIVE_TITLE_ROW_COUNT; ++index) {
                    *(void **)((uint8_t *)row_controller +
                        NATIVE_TITLE_ROW_POINTER_OFFSET +
                            index * sizeof(void *)) =
                                roster_native_animated_rows[index];
                }
            }
        }
    }
    if (original_front_end_render != NULL) {
        original_front_end_render(controller);
    }
    if (animated_rows_staged) {
        for (index = 0u; index < NATIVE_TITLE_ROW_COUNT; ++index) {
            *(void **)((uint8_t *)row_controller +
                NATIVE_TITLE_ROW_POINTER_OFFSET +
                    index * sizeof(void *)) = saved_title_rows[index];
        }
    }
    if (roster_native_screen && controller != NULL && game_base != NULL &&
        *(void **)controller == (void *)(game_base + RVA_PC_FRONT_END_VTABLE)) {
        native_roster_submit_page();
    }
}

static void roster_begin_native_new_game(
    void *controller,
    unsigned int phase,
    unsigned int event,
    unsigned int argument
) {
    roster_resume_committed = FALSE;
    if (!native_roster_snapshot_original_menu(controller)) {
        SudekiMpLogWrite(
            "cleanroom_menu event=native_roster status=rejected "
            "reason=original_menu_snapshot_failed\r\n");
        return;
    }
    roster_pending_controller = controller;
    roster_pending_phase = phase;
    roster_pending_event = event;
    roster_pending_argument = argument;
    roster_waiting_new_game = TRUE;
    roster_locked = FALSE;
    native_save_page_input_suppression_logged = FALSE;
    roster_native_screen = TRUE;
    roster_native_screen_kind = NATIVE_ROSTER_MODE;
    roster_native_selection = 0u;
    menu_open = FALSE;
    menu_texture_dirty = FALSE;
    native_roster_start_page_transition(TRUE);
    /*
     * Do not request the shipped New Game fade (state 10) until the
     * experimental roster has proved that it can take over the native page.
     *
     * The title dispatcher and controller state are distinct.  On a real
     * Windows title flow the activation may arrive through dispatcher phase
     * 5 while controller+0x44 is still state 2.  The roster page requires a
     * stricter state-5/load-page contract, so this is an expected fail-open
     * boundary rather than permission to fade the title out.  Requesting
     * state 10 first left the native front end black after a rejected page
     * takeover, even though the original StartNewGame action was later
     * replayed.  Keeping the title untouched lets that original action run
     * exactly as vanilla when the roster experiment is unavailable.
     */
    if (!native_roster_enter_page_state(controller)) {
        roster_native_screen = FALSE;
        roster_native_screen_kind = NATIVE_ROSTER_NONE;
        roster_waiting_new_game = FALSE;
        roster_pending_controller = NULL;
        (void)native_roster_restore_original_menu(controller);
        native_roster_restore_vanilla_items(controller);
        SudekiMpLogWrite(
            "cleanroom_menu event=native_roster status=rejected "
            "reason=independent_page_activation_failed "
            "fallback=vanilla_new_game_title_unchanged\r\n");
        return;
    }
    if (!native_roster_create_animated_rows()) {
        SudekiMpLogWrite(
            "cleanroom_menu event=native_roster_animated_rows "
            "status=unavailable fallback=native_font_text_only\r\n");
    }
    native_roster_rebuild_from_native_menu(controller);
    SudekiMpLogFormat(
        "cleanroom_menu event=native_roster state=native_mode reason=new_game "
        "phase=%lu event_code=%lu p1=%s p2=%s "
        "presentation=native_state10_independent_state6_sfe_option_rows\r\n",
        (unsigned long)phase,
        (unsigned long)event,
        roster_actor_label(roster_player_one),
        roster_actor_label(roster_player_two)
    );
}

void __attribute__((noinline)) cleanroom_front_end_state_trace(
    void *controller,
    unsigned int state
) {
    static void *last_controller;
    static unsigned int last_state = ~0u;

    if (!roster_mode || controller == NULL ||
        (controller == last_controller && state == last_state)) {
        return;
    }
    last_controller = controller;
    last_state = state;
    SudekiMpLogFormat(
        "cleanroom_menu event=native_front_end_state controller=%p state=%lu\\r\\n",
        controller, (unsigned long)state);
}

/* FUN_004A0F40 receives its state in EAX and its controller as the single
 * stack argument.  Preserve both exactly while adding the trace, then tail
 * jump to the original trampoline so its ret 4 and all flags remain native. */
__attribute__((naked, noinline, used))
static void cleanroom_front_end_state_update_entry(void) {
    __asm__ volatile(
        "pushal\n\t"
        "movl 28(%esp), %eax\n\t"
        "movl 36(%esp), %ecx\n\t"
        "pushl %eax\n\t"
        "pushl %ecx\n\t"
        "call _cleanroom_front_end_state_trace\n\t"
        "addl $8, %esp\n\t"
        "popal\n\t"
        "movl _original_front_end_state_update, %eax\n\t"
        "jmp *%eax\n\t"
    );
}

static unsigned int __attribute__((thiscall)) cleanroom_front_end_action(
    void *controller,
    unsigned int phase,
    unsigned int event,
    unsigned int argument
) {
    const char *action = NULL;
    unsigned int actor;
    unsigned int result;
    static unsigned int last_dispatch_phase = ~0u;
    static unsigned int last_dispatch_event = ~0u;

    /* The title dispatcher has several phases (keyboard, mouse, and
     * controller activation).  Keep a transition-only trace while the native
     * roster is enabled so a build-specific phase cannot silently fall through
     * to the original New Game path. */
    if (roster_mode && !roster_replaying_new_game &&
        (phase != last_dispatch_phase || event != last_dispatch_event)) {
        SudekiMpLogFormat(
            "cleanroom_menu event=native_roster_dispatch controller=%p "
            "phase=%lu event_code=%lu argument=%lu stage=%lu mode=%lu "
            "selection=%lu count=%lu action=%s\\r\\n",
            controller,
            (unsigned long)phase,
            (unsigned long)event,
            (unsigned long)argument,
            *(unsigned int *)((uint8_t *)controller + 0x48u),
            *(unsigned int *)((uint8_t *)controller + 0x44u),
            *(unsigned int *)((uint8_t *)controller + 0x17d4u),
            *(unsigned int *)((uint8_t *)controller + 0x17d8u),
            action == NULL ? "<none>" : action);
        last_dispatch_phase = phase;
        last_dispatch_event = event;
    }

    if (!roster_mode || roster_replaying_new_game) {
        return original_front_end_action == NULL ? 0u :
            original_front_end_action(controller, phase, event, argument);
    }

    if (!roster_native_screen) {
        if (native_front_end_activation(phase, event, argument)) {
            action = front_end_selected_action(controller);
            if (action != NULL && _stricmp(action, "StartNewGame") == 0) {
                if (roster_resume_committed &&
                    controller == roster_resume_controller) {
                    SudekiMpLogWrite(
                        "cleanroom_menu event=native_roster "
                        "state=resume_duplicate_suppressed "
                        "reason=native_new_game_already_replayed\r\n");
                    return 1u;
                }
                roster_begin_native_new_game(
                    controller, phase, event, argument);
                if (roster_native_screen) {
                    /* Consume the real phase-5 activation.  The base title
                     * controller remains in its resident state 5 while our
                     * replacement records become the active native page. */
                    return 1u;
                }
            }
            if (action != NULL && _stricmp(action, "Options") == 0) {
                trace_native_options_page(controller, "before");
                result = original_front_end_action == NULL ? 0u :
                    original_front_end_action(
                        controller, phase, event, argument);
                trace_native_options_page(controller, "after");
                return result;
            }
        }
        return original_front_end_action == NULL ? 0u :
            original_front_end_action(controller, phase, event, argument);
    }

    native_roster_arm_confirm_after_release();

    if (native_roster_navigation(controller, phase, event)) {
        /* Calling the original navigation path would invoke FUN_004A1950 and
         * rebuild the vanilla title records over our resident native page. */
        return 1u;
    }

    if (!native_front_end_activation(phase, event, argument)) {
        /* This is now a genuine controller subpage.  Passing any event back
         * to FUN_004A0360 would either navigate state 5 again or delegate to
         * a native page that does not own our choices. */
        return 1u;
    }

    if (!roster_confirm_input_armed ||
        !native_roster_confirm_input_down(event)) {
        SudekiMpLogFormat(
            "cleanroom_menu event=native_roster state=confirm_suppressed "
            "reason=stale_or_released_input armed=%s phase=%lu event_code=%lu "
            "argument=%lu\r\n",
            roster_confirm_input_armed ? "true" : "false",
            (unsigned long)phase,
            (unsigned long)event,
            (unsigned long)argument);
        return 1u;
    }

    action = native_roster_selected_action();
    if (action == NULL) {
        return 0u;
    }
    if (_stricmp(action, "SudekiMPRosterBack") == 0) {
        if (roster_native_screen_kind == NATIVE_ROSTER_MODE) {
            (void)native_roster_back_to_title(controller);
        }
        else if (roster_native_screen_kind == NATIVE_ROSTER_PLAYER_ONE) {
            roster_native_screen_kind = NATIVE_ROSTER_MODE;
            roster_native_selection = 1u;
            native_roster_start_page_transition(FALSE);
            native_roster_rebuild_from_native_menu(controller);
        }
        else if (roster_native_screen_kind == NATIVE_ROSTER_PLAYER_TWO) {
            roster_native_screen_kind = NATIVE_ROSTER_PLAYER_ONE;
            roster_native_selection =
                roster_display_card_for_actor(roster_player_one);
            native_roster_start_page_transition(FALSE);
            native_roster_rebuild_from_native_menu(controller);
        }
        else if (roster_native_screen_kind == NATIVE_ROSTER_CONFIRM) {
            roster_native_screen_kind = NATIVE_ROSTER_PLAYER_TWO;
            roster_native_selection =
                roster_display_card_for_actor(roster_player_two);
            native_roster_start_page_transition(FALSE);
            native_roster_rebuild_from_native_menu(controller);
        }
        else if (roster_native_screen_kind == NATIVE_ROSTER_SETTINGS) {
            roster_native_screen_kind = NATIVE_ROSTER_MODE;
            roster_native_selection = 2u;
            native_roster_start_page_transition(FALSE);
            native_roster_rebuild_from_native_menu(controller);
        }
        return 1u;
    }
    if (_stricmp(action, "SudekiMPSinglePlayer") == 0) {
        if (!SudekiMpSplitScreenClearRosterTypes()) {
            SudekiMpLogFormat(
                "cleanroom_menu event=native_roster status=rejected "
                "reason=single_player_runtime_release_failed error=%lu\r\n",
                (unsigned long)GetLastError());
            return 0u;
        }
        roster_coop_profile = FALSE;
        roster_talos_tuning_enabled = FALSE;
        roster_save_persistence();
        if (!native_roster_restore_original_menu(controller)) {
            SudekiMpLogWrite(
                "cleanroom_menu event=native_roster status=rejected "
                "reason=single_player_restore_failed\r\n");
            return 0u;
        }
        native_roster_release_animated_rows();
        native_roster_release_options_rows(controller);
        if (!native_roster_leave_page_state(controller)) {
            SudekiMpLogWrite(
                "cleanroom_menu event=native_roster status=rejected "
                "reason=single_player_page_restore_failed\r\n");
            return 0u;
        }
        roster_native_screen = FALSE;
        roster_native_screen_kind = NATIVE_ROSTER_NONE;
        roster_waiting_new_game = FALSE;
        native_roster_restore_vanilla_items(controller);
        roster_resume_committed = TRUE;
        roster_resume_controller = controller;
        roster_replaying_new_game = TRUE;
        result = original_front_end_action == NULL ? 0u :
            original_front_end_action(roster_pending_controller,
                roster_pending_phase, roster_pending_event,
                roster_pending_argument);
        roster_replaying_new_game = FALSE;
        roster_pending_controller = NULL;
        return result;
    }
    if (_stricmp(action, "SudekiMPCoop") == 0) {
        roster_native_screen_kind = NATIVE_ROSTER_PLAYER_ONE;
        roster_native_selection =
            roster_display_card_for_actor(roster_player_one);
        native_roster_start_page_transition(FALSE);
        native_roster_rebuild_from_native_menu(controller);
        return 1u;
    }
    if (_stricmp(action, "SudekiMPSettings") == 0) {
        roster_native_screen_kind = NATIVE_ROSTER_SETTINGS;
        roster_native_selection = 0u;
        native_roster_start_page_transition(FALSE);
        native_roster_rebuild_from_native_menu(controller);
        return 1u;
    }
    if (_stricmp(action, "SudekiMPTalosToggle") == 0) {
        roster_talos_tuning_enabled = !roster_talos_tuning_enabled;
        if (roster_talos_tuning_enabled) {
            roster_coop_profile = TRUE;
        }
        roster_save_persistence();
        native_roster_refresh_screen(controller);
        return 1u;
    }
    if (_stricmp(action, "SudekiMPTalosHealth") == 0) {
        roster_talos_health_scale = roster_talos_health_scale >= 4u ?
            1u : roster_talos_health_scale + 1u;
        roster_save_persistence();
        native_roster_refresh_screen(controller);
        return 1u;
    }
    if (_stricmp(action, "SudekiMPTalosStagger") == 0) {
        roster_talos_stagger_limit = roster_talos_stagger_limit == 6u ? 10u :
            (roster_talos_stagger_limit == 10u ? 14u :
            (roster_talos_stagger_limit == 14u ? 18u : 6u));
        roster_save_persistence();
        native_roster_refresh_screen(controller);
        return 1u;
    }
    if (_stricmp(action, "SudekiMPTalosWindow") == 0) {
        roster_talos_stagger_window = roster_talos_stagger_window >= 20u ?
            5u : roster_talos_stagger_window + 5u;
        roster_save_persistence();
        native_roster_refresh_screen(controller);
        return 1u;
    }
    if (_stricmp(action, "SudekiMPP1Ailish") == 0 ||
        _stricmp(action, "SudekiMPP1Tal") == 0 ||
        _stricmp(action, "SudekiMPP1Buki") == 0 ||
        _stricmp(action, "SudekiMPP1Elco") == 0) {
        actor = _stricmp(action, "SudekiMPP1Ailish") == 0 ? 0u :
            (_stricmp(action, "SudekiMPP1Tal") == 0 ? 1u :
            (_stricmp(action, "SudekiMPP1Buki") == 0 ? 2u : 3u));
        roster_player_one = (unsigned int)roster_display_actor(actor);
        roster_native_screen_kind = NATIVE_ROSTER_PLAYER_TWO;
        roster_native_selection = roster_first_available_player_two_card();
        native_roster_start_page_transition(FALSE);
        native_roster_rebuild_from_native_menu(controller);
        return 1u;
    }
    if (_stricmp(action, "SudekiMPP2Ailish") == 0 ||
        _stricmp(action, "SudekiMPP2Tal") == 0 ||
        _stricmp(action, "SudekiMPP2Buki") == 0 ||
        _stricmp(action, "SudekiMPP2Elco") == 0) {
        actor = _stricmp(action, "SudekiMPP2Ailish") == 0 ? 0u :
            (_stricmp(action, "SudekiMPP2Tal") == 0 ? 1u :
            (_stricmp(action, "SudekiMPP2Buki") == 0 ? 2u : 3u));
        actor = (unsigned int)roster_display_actor(actor);
        if (actor == roster_player_one) {
            SudekiMpLogWrite(
                "cleanroom_menu event=native_roster status=rejected "
                "reason=duplicate_character\r\n");
            return 1u;
        }
        roster_player_two = actor;
        roster_native_screen_kind = NATIVE_ROSTER_CONFIRM;
        roster_native_selection = 2u;
        native_roster_start_page_transition(FALSE);
        native_roster_rebuild_from_native_menu(controller);
        return 1u;
    }
    if (_stricmp(action, "SudekiMPRosterP1") == 0) {
        roster_native_screen_kind = NATIVE_ROSTER_PLAYER_ONE;
        roster_native_selection =
            roster_display_card_for_actor(roster_player_one);
        native_roster_start_page_transition(FALSE);
        native_roster_rebuild_from_native_menu(controller);
        return 1u;
    }
    if (_stricmp(action, "SudekiMPRosterP2") == 0) {
        roster_native_screen_kind = NATIVE_ROSTER_PLAYER_TWO;
        roster_native_selection =
            roster_display_card_for_actor(roster_player_two);
        native_roster_start_page_transition(FALSE);
        native_roster_rebuild_from_native_menu(controller);
        return 1u;
    }
    if (_stricmp(action, "SudekiMPRosterLock") == 0) {
        if (roster_player_one == roster_player_two ||
            !SudekiMpSplitScreenSetRosterTypes(
                roster_actor_type(roster_player_one),
                roster_actor_type(roster_player_two))) {
            SudekiMpLogWrite(
                "cleanroom_menu event=native_roster status=rejected "
                "reason=role_binding_failed\r\n");
            return 1u;
        }
        roster_locked = TRUE;
        roster_coop_profile = TRUE;
        roster_save_persistence();
        if (!native_roster_restore_original_menu(controller)) {
            roster_locked = FALSE;
            SudekiMpLogWrite(
                "cleanroom_menu event=native_roster status=rejected "
                "reason=coop_restore_failed\r\n");
            return 1u;
        }
        native_roster_release_animated_rows();
        native_roster_release_options_rows(controller);
        if (!native_roster_leave_page_state(controller)) {
            roster_locked = FALSE;
            SudekiMpLogWrite(
                "cleanroom_menu event=native_roster status=rejected "
                "reason=coop_page_restore_failed\r\n");
            return 1u;
        }
        roster_native_screen = FALSE;
        roster_native_screen_kind = NATIVE_ROSTER_NONE;
        roster_waiting_new_game = FALSE;
        native_roster_restore_vanilla_items(controller);
        roster_resume_committed = TRUE;
        roster_resume_controller = controller;
        roster_replaying_new_game = TRUE;
        SudekiMpLogFormat(
            "cleanroom_menu event=native_roster status=locked "
            "p1=%s p2=%s policy=native_menu_then_native_resume\r\n",
            roster_actor_label(roster_player_one),
            roster_actor_label(roster_player_two));
        result = original_front_end_action == NULL ? 0u :
            original_front_end_action(roster_pending_controller,
                roster_pending_phase, roster_pending_event,
                roster_pending_argument);
        roster_replaying_new_game = FALSE;
        roster_pending_controller = NULL;
        return result;
    }
    /* Unknown mod actions are consumed rather than falling through to the
     * vanilla catch-all branch, which transitions state 5 to state 10. */
    return 1u;
}

static void roster_resume_native_new_game(void) {
    if (!roster_waiting_new_game || !roster_locked ||
        original_front_end_action == NULL || roster_pending_controller == NULL) {
        return;
    }
    roster_save_persistence();
    (void)SudekiMpSplitScreenSetRosterTypes(
        roster_actor_type(roster_player_one),
        roster_actor_type(roster_player_two)
    );
    roster_waiting_new_game = FALSE;
    menu_open = FALSE;
    menu_texture_dirty = TRUE;
    roster_replaying_new_game = TRUE;
    SudekiMpLogFormat(
        "cleanroom_menu event=native_roster state=resume "
        "action=StartNewGame p1=%s p2=%s policy=native_action_replay\r\n",
        roster_actor_label(roster_player_one),
        roster_actor_label(roster_player_two)
    );
    (void)original_front_end_action(
        roster_pending_controller,
        roster_pending_phase,
        roster_pending_event,
        roster_pending_argument
    );
    roster_replaying_new_game = FALSE;
    roster_pending_controller = NULL;
}

static BOOL owns_foreground(void) {
    HWND foreground = GetForegroundWindow();
    DWORD process_id = 0u;

    if (foreground != NULL) {
        GetWindowThreadProcessId(foreground, &process_id);
    }
    return process_id == GetCurrentProcessId();
}

static void reset_save_book_vote_input(void) {
    ZeroMemory(&save_book_vote_input, sizeof(save_book_vote_input));
}

static void begin_save_book_vote_input(
    const SudekiMpSaveBookVoteSnapshot *snapshot
) {
    SudekiMpInputBridgeState raw_input;
    BOOL raw_available;

    reset_save_book_vote_input();
    raw_available = SudekiMpInputBridgePollRaw(&raw_input);
    SudekiMpSaveBookVoteInputBegin(
        &save_book_vote_input.player_two,
        snapshot->serial,
        raw_available,
        raw_available ? &raw_input : NULL);
    save_book_vote_input.overlay_acknowledged =
        snapshot->state == SUDEKIMP_SAVE_BOOK_VOTE_WAITING;
    save_book_vote_input.player_one_cancel_was_down =
        (GetAsyncKeyState(VK_ESCAPE) & 0x8000) != 0;
    SudekiMpLogFormat(
        "save_book_vote event=input_fence phase=open serial=%lu "
        "p1_escape_down=%u p2_baseline=%s p2_sequence=%lu "
        "policy=p1_release_then_rising_escape_p2_newer_neutral_then_a_or_b\r\n",
        (unsigned long)snapshot->serial,
        save_book_vote_input.player_one_cancel_was_down ? 1u : 0u,
        save_book_vote_input.player_two.baseline_valid ?
            "captured" : "awaiting",
        (unsigned long)save_book_vote_input.player_two.last_sequence);
}

static void service_save_book_vote_input(
    const SudekiMpSaveBookVoteSnapshot *snapshot
) {
    SudekiMpInputBridgeState raw_input;
    SudekiMpSaveBookVoteInputAction player_two_action;
    BOOL player_one_cancel_down;
    BOOL foreground;

    if (snapshot == NULL || !snapshot->active || snapshot->serial == 0u) {
        reset_save_book_vote_input();
        return;
    }
    if (save_book_vote_input.player_two.serial != snapshot->serial) {
        begin_save_book_vote_input(snapshot);
    }
    if (snapshot->state == SUDEKIMP_SAVE_BOOK_VOTE_WAITING) {
        save_book_vote_input.overlay_acknowledged = TRUE;
    }

    player_one_cancel_down =
        (GetAsyncKeyState(VK_ESCAPE) & 0x8000) != 0;
    foreground = owns_foreground();
    if (foreground &&
        save_book_vote_input.overlay_acknowledged &&
        player_one_cancel_down &&
        !save_book_vote_input.player_one_cancel_was_down) {
        if (SudekiMpSaveBookRespondVote(
                snapshot->serial, 0u, FALSE)) {
            SudekiMpLogFormat(
                "save_book_vote event=input player=1 action=veto "
                "source=escape serial=%lu fence=release_then_rising\r\n",
                (unsigned long)snapshot->serial);
            save_book_vote_input.player_one_cancel_was_down =
                player_one_cancel_down;
            return;
        }
    }
    save_book_vote_input.player_one_cancel_was_down =
        player_one_cancel_down;

    if (!foreground || (snapshot->participant_mask & 0x02u) == 0u ||
        !SudekiMpInputBridgePollRaw(&raw_input)) {
        return;
    }
    player_two_action = SudekiMpSaveBookVoteInputAdvance(
        &save_book_vote_input.player_two,
        snapshot->serial,
        &raw_input,
        save_book_vote_input.overlay_acknowledged);
    if (player_two_action == SUDEKIMP_SAVE_BOOK_VOTE_INPUT_ARMED) {
        SudekiMpLogFormat(
            "save_book_vote event=input_fence phase=armed player=2 "
            "serial=%lu sequence=%lu "
            "policy=post_open_newer_neutral_observed\r\n",
            (unsigned long)snapshot->serial,
            (unsigned long)raw_input.sequence);
        return;
    }
    if (player_two_action == SUDEKIMP_SAVE_BOOK_VOTE_INPUT_VETO) {
        if (SudekiMpSaveBookRespondVote(
                snapshot->serial, 1u, FALSE)) {
            SudekiMpLogFormat(
                "save_book_vote event=input player=2 action=veto "
                "source=controller_b serial=%lu sequence=%lu\r\n",
                (unsigned long)snapshot->serial,
                (unsigned long)raw_input.sequence);
        }
    } else if (player_two_action ==
            SUDEKIMP_SAVE_BOOK_VOTE_INPUT_ACCEPT) {
        if (SudekiMpSaveBookRespondVote(
                snapshot->serial, 1u, TRUE)) {
            SudekiMpLogFormat(
                "save_book_vote event=input player=2 action=accept "
                "source=controller_a serial=%lu sequence=%lu\r\n",
                (unsigned long)snapshot->serial,
                (unsigned long)raw_input.sequence);
        }
    }
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
        infinite_jetpack_fuel_valid = FALSE;
        multiplayer_active = FALSE;
        multiplayer_input_ready = FALSE;
        last_status_update = 0u;
        return;
    }
    if (zone_traversal_mode && zone_traversal_waiting &&
        SudekiMpZoneTraversalWorldMatches(zone_traversal_waiting_world)) {
        zone_traversal_waiting = FALSE;
        zone_traversal_waiting_since = 0u;
        zone_traversal_waiting_world[0] = '\0';
        menu_texture_dirty = TRUE;
        SudekiMpLogFormat(
            "zone_traversal event=world_ready world=%s\r\n",
            SudekiMpZoneTraversalCurrentWorld());
    } else if (zone_traversal_mode && zone_traversal_waiting &&
        zone_traversal_waiting_since != 0u &&
        (DWORD)(now - zone_traversal_waiting_since) > 15000u) {
        SudekiMpLogFormat(
            "zone_traversal event=world_ready status=timeout "
            "requested=%s\r\n",
            zone_traversal_waiting_world[0] == '\0' ?
                "<unknown>" : zone_traversal_waiting_world);
        zone_traversal_waiting = FALSE;
        zone_traversal_waiting_since = 0u;
        zone_traversal_waiting_world[0] = '\0';
        menu_texture_dirty = TRUE;
    }
    if (integrated_multiplayer_mode && !coop_lobby_prompted &&
        !coop_role_lock_active) {
        coop_lobby_prompted = TRUE;
        menu_open = TRUE;
        selected_item = 0u;
        menu_texture_dirty = TRUE;
        SudekiMpLogWrite(
            "cleanroom_menu event=co_op_lobby state=open "
            "policy=choose_p2_then_ready_before_split\r\n"
        );
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
    if (SudekiMpCleanroomEngineInfiniteJetpackFuel(&mode)) {
        if (!infinite_jetpack_fuel_valid ||
            infinite_jetpack_fuel != mode) {
            infinite_jetpack_fuel = mode;
            infinite_jetpack_fuel_valid = TRUE;
            menu_texture_dirty = TRUE;
        }
    } else if (infinite_jetpack_fuel_valid) {
        infinite_jetpack_fuel_valid = FALSE;
        menu_texture_dirty = TRUE;
    }
    if (integrated_multiplayer_mode) {
        BOOL requested = SudekiMpControlSeparationPlayerTwoRequested();
        BOOL active = SudekiMpControlSeparationPlayerTwoActive();
        BOOL input_ready = SudekiMpControlSeparationInputReady();
        BOOL participation_requested =
            SudekiMpSplitScreenRosterParticipationRequested();

        if (multiplayer_requested != requested ||
            multiplayer_active != active ||
            multiplayer_input_ready != input_ready ||
            multiplayer_participation_requested !=
                participation_requested) {
            multiplayer_requested = requested;
            multiplayer_active = active;
            multiplayer_input_ready = input_ready;
            multiplayer_participation_requested =
                participation_requested;
            menu_texture_dirty = TRUE;
            player_two_badge_dirty = TRUE;
        }
        if (coop_role_lock_active != SudekiMpSplitScreenRolesLocked()) {
            coop_role_lock_active = SudekiMpSplitScreenRolesLocked();
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

static BOOL rising_key(unsigned int slot, UINT key);

static unsigned int traversal_interior_count_for_world(
    const char *world,
    unsigned int *first_index
) {
    unsigned int index;
    unsigned int count = 0u;
    unsigned int first = 0u;

    if (world == NULL) {
        if (first_index != NULL) {
            *first_index = 0u;
        }
        return 0u;
    }
    for (index = 0u; index < sizeof(traversal_interiors) /
            sizeof(traversal_interiors[0]); ++index) {
        if (_stricmp(traversal_interiors[index].world, world) != 0) {
            continue;
        }
        if (count == 0u) {
            first = index;
        }
        ++count;
    }
    if (first_index != NULL) {
        *first_index = first;
    }
    return count;
}

static void activate_traversal_selection(void) {
    const char *current_world = SudekiMpZoneTraversalCurrentWorld();

    if (zone_traversal_waiting ||
        (LONG)(GetTickCount() - zone_traversal_transition_guard_until) < 0) {
        SudekiMpLogWrite(
            "zone_traversal action=ignored reason=transition_pending\r\n");
        return;
    }

    if (zone_traversal_page == ZONE_TRAVERSAL_PAGE_WORLDS) {
        const SudekiMpTraversalWorld *destination =
            &traversal_worlds[zone_traversal_selection];

        if (SudekiMpZoneTraversalWorldMatches(destination->name)) {
            SudekiMpLogFormat(
                "zone_traversal action=set_world status=already_current "
                "world=%s\r\n",
                destination->name);
            menu_texture_dirty = TRUE;
            return;
        }

        if (!zone_traversal_direct_persistent_enabled ||
            !SudekiMpZoneTraversalArrivalContextReady(
                destination->name, NULL)) {
            SudekiMpLogFormat(
                "zone_traversal action=set_world status=rejected "
                "reason=awaiting_cached_native_savepoint_context zone=%s\r\n",
                destination->name);
            return;
        }

        if (!SudekiMpZoneTraversalSwitchWorld(destination->name)) {
            SudekiMpLogFormat(
                "zone_traversal action=switch_world status=rejected "
                "zone=%s error=%lu\r\n",
                destination->name, (unsigned long)GetLastError());
            return;
        }
        lstrcpynA(zone_traversal_waiting_world, destination->name,
            sizeof(zone_traversal_waiting_world));
        zone_traversal_waiting = TRUE;
        zone_traversal_waiting_since = GetTickCount();
        zone_traversal_transition_guard_until = GetTickCount() + 1200u;
        menu_texture_dirty = TRUE;
        return;
    }
    {
        unsigned int first_index;
        unsigned int count = traversal_interior_count_for_world(
            current_world, &first_index);
        const SudekiMpTraversalInterior *destination;

        if (count == 0u || zone_traversal_selection >= count) {
            SudekiMpLogFormat(
                "zone_traversal action=enter_temporary status=rejected "
                "reason=no_confirmed_interiors current_world=%s\r\n",
                current_world == NULL ? "<unknown>" : current_world);
            return;
        }
        destination = &traversal_interiors[first_index +
            zone_traversal_selection];

        if (!zone_traversal_direct_temporary_enabled ||
            !SudekiMpZoneTraversalArrivalContextReady(
                current_world, destination->name)) {
            SudekiMpLogFormat(
                "zone_traversal action=enter_temporary status=rejected "
                "reason=awaiting_cached_native_savepoint_context world=%s "
                "zone=%s\r\n",
                current_world == NULL ? "<unknown>" : current_world,
                destination->name);
            return;
        }
        if (!SudekiMpZoneTraversalEnterTemporary(destination->name)) {
            SudekiMpLogFormat(
                "zone_traversal action=enter_temporary status=rejected "
                "world=%s zone=%s error=%lu\r\n",
                current_world == NULL ? "<unknown>" : current_world,
                destination->name, (unsigned long)GetLastError());
            return;
        }
        menu_open = FALSE;
        zone_traversal_transition_guard_until = GetTickCount() + 1200u;
        menu_texture_dirty = TRUE;
    }
}

static void poll_traversal_input(
    BOOL up,
    BOOL down,
    BOOL left,
    BOOL right
) {

    if (left && zone_traversal_page == ZONE_TRAVERSAL_PAGE_INTERIORS) {
        zone_traversal_page = ZONE_TRAVERSAL_PAGE_WORLDS;
        zone_traversal_selection = 0u;
        menu_texture_dirty = TRUE;
    }
    if (right && zone_traversal_page == ZONE_TRAVERSAL_PAGE_WORLDS) {
        const char *world = SudekiMpZoneTraversalCurrentWorld();
        if (!zone_traversal_waiting && world != NULL &&
            traversal_interior_count_for_world(
                world, NULL) != 0u) {
            zone_traversal_page = ZONE_TRAVERSAL_PAGE_INTERIORS;
            zone_traversal_selection = 0u;
            menu_texture_dirty = TRUE;
        }
    }
    if (zone_traversal_page == ZONE_TRAVERSAL_PAGE_WORLDS) {
        unsigned int count = sizeof(traversal_worlds) /
            sizeof(traversal_worlds[0]);
        if (up) {
            zone_traversal_selection = zone_traversal_selection == 0u ?
                count - 1u : zone_traversal_selection - 1u;
            menu_texture_dirty = TRUE;
        }
        if (down) {
            zone_traversal_selection = (zone_traversal_selection + 1u) % count;
            menu_texture_dirty = TRUE;
        }
    }
    else {
        const char *world = SudekiMpZoneTraversalCurrentWorld();
        unsigned int count = traversal_interior_count_for_world(world, NULL);
        if (count != 0u && up) {
            zone_traversal_selection = zone_traversal_selection == 0u ?
                count - 1u : zone_traversal_selection - 1u;
            menu_texture_dirty = TRUE;
        }
        if (count != 0u && down) {
            zone_traversal_selection = (zone_traversal_selection + 1u) % count;
            menu_texture_dirty = TRUE;
        }
    }
}

static void activate_selected_item(void) {
    float position[3];
    BOOL accepted = FALSE;
    BOOL mode;

    if (zone_traversal_mode) {
        activate_traversal_selection();
        return;
    }

    if (selected_item == MENU_CLOSE_INDEX) {
        menu_open = FALSE;
        return;
    }
    if (selected_item == MENU_COOP_READY_INDEX) {
        void *player_one;
        void *player_two;
        unsigned int index;

        if (coop_role_lock_active || !integrated_multiplayer_mode) {
            return;
        }
        if (coop_selected_actor == SUDEKIMP_CLEANROOM_AILISH ||
            !item_present[coop_selected_actor]) {
            coop_ready_failed_until = GetTickCount() + 2500u;
            menu_texture_dirty = TRUE;
            SudekiMpLogWrite(
                "cleanroom_menu event=co_op_ready status=rejected "
                "reason=no_p2_candidate\r\n"
            );
            return;
        }
        for (index = 0u; index < MENU_ACTOR_COUNT; ++index) {
            if (index != SUDEKIMP_CLEANROOM_AILISH &&
                index != (unsigned int)coop_selected_actor &&
                item_present[index]) {
                coop_ready_failed_until = GetTickCount() + 2500u;
                menu_texture_dirty = TRUE;
                SudekiMpLogWrite(
                    "cleanroom_menu event=co_op_ready status=rejected "
                    "reason=multiple_p2_candidates\r\n"
                );
                return;
            }
        }
        player_one = SudekiMpCleanroomEngineActorEntity(
            SUDEKIMP_CLEANROOM_AILISH
        );
        player_two = SudekiMpCleanroomEngineActorEntity(coop_selected_actor);
        accepted = player_one != NULL && player_two != NULL &&
            SudekiMpControlSeparationRequestPlayerTwo(TRUE) &&
            SudekiMpSplitScreenSetRuntimeEnabled(TRUE) &&
            SudekiMpSplitScreenLockRoles(player_one, player_two);
        if (!accepted) {
            (void)SudekiMpControlSeparationRequestPlayerTwo(FALSE);
            (void)SudekiMpSplitScreenSetRuntimeEnabled(FALSE);
            coop_ready_failed_until = GetTickCount() + 2500u;
            menu_texture_dirty = TRUE;
            SudekiMpLogWrite(
                "cleanroom_menu event=co_op_ready status=rejected "
                "reason=role_lock_or_split_enable_failed\r\n"
            );
            return;
        }
        (void)SudekiMpControlSeparationSetRoleLock(TRUE);
        coop_role_lock_active = TRUE;
        menu_open = FALSE;
        menu_texture_dirty = TRUE;
        player_two_badge_dirty = TRUE;
        SudekiMpLogFormat(
            "cleanroom_menu event=co_op_ready status=locked "
            "p1=Ailish p2=%s policy=roles_immutable_until_uninstall\r\n",
            SudekiMpCleanroomActorLabel(coop_selected_actor)
        );
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
        if (coop_role_lock_active) {
            SudekiMpLogWrite(
                "cleanroom_menu event=multiplayer_toggle status=rejected "
                "reason=co_op_roles_locked\r\n"
            );
            return;
        }
        SudekiMpLogWrite(
            "cleanroom_menu event=multiplayer_toggle status=rejected "
            "reason=use_co_op_ready\r\n"
        );
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
    if (selected_item == MENU_INFINITE_JETPACK_INDEX) {
        if (SudekiMpCleanroomEngineInfiniteJetpackFuel(&mode)) {
            accepted = SudekiMpCleanroomEngineSetInfiniteJetpackFuel(!mode);
        }
        infinite_jetpack_fuel_valid =
            SudekiMpCleanroomEngineInfiniteJetpackFuel(
                &infinite_jetpack_fuel);
        menu_texture_dirty = TRUE;
        if (!accepted) {
            SudekiMpLogWrite(
                "cleanroom_menu event=infinite_jetpack_fuel "
                "status=rejected\r\n"
            );
        }
        return;
    }
    if (selected_item == SUDEKIMP_CLEANROOM_AILISH ||
        selected_item > MENU_DUMMY_INDEX ||
        pending_actions[selected_item] != SUDEKIMP_PENDING_NONE) {
        return;
    }
    if (coop_role_lock_active) {
        SudekiMpLogWrite(
            "cleanroom_menu event=actor_toggle status=rejected "
            "reason=co_op_roles_locked\r\n"
        );
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
        if (integrated_multiplayer_mode &&
            selected_item != SUDEKIMP_CLEANROOM_AILISH &&
            item_present[selected_item]) {
            coop_selected_actor = (SudekiMpCleanroomActor)selected_item;
            menu_texture_dirty = TRUE;
            SudekiMpLogFormat(
                "cleanroom_menu event=co_op_candidate actor=%s "
                "status=selected\r\n",
                SudekiMpCleanroomActorLabel(coop_selected_actor)
            );
            return;
        }
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
                coop_selected_actor = (SudekiMpCleanroomActor)selected_item;
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
    BOOL left = rising_key(6u, VK_LEFT);
    BOOL right = rising_key(7u, VK_RIGHT);
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
    if (zone_traversal_mode) {
        poll_traversal_input(up, down, left, right);
        if (activate) {
            activate_selected_item();
        }
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

static BOOL roster_key(unsigned int slot, UINT key) {
    BOOL down = (GetAsyncKeyState((int)key) & 0x8000) != 0;
    BOOL rising = down && !key_was_down[slot];
    key_was_down[slot] = down;
    return rising;
}

static void clear_roster_resume_guard_after_gameplay_handoff(void) {
    unsigned int actor;

    if (!roster_resume_committed) {
        return;
    }
    for (actor = 0u; actor < MENU_ACTOR_COUNT; ++actor) {
        if (!SudekiMpCleanroomEngineActorPresent(
                (SudekiMpCleanroomActor)actor)) {
            continue;
        }
        roster_resume_committed = FALSE;
        roster_resume_controller = NULL;
        SudekiMpLogFormat(
            "cleanroom_menu event=native_roster state=resume_guard_cleared "
            "reason=gameplay_actor_present actor=%s\r\n",
            SudekiMpCleanroomActorLabel((SudekiMpCleanroomActor)actor));
        return;
    }
}

static void poll_roster_input(void) {
    unsigned int *selected = roster_cursor == 0u ?
        &roster_player_one : &roster_player_two;
    if (roster_native_screen || !roster_waiting_new_game || !owns_foreground()) {
        return;
    }
    if (roster_locked) {
        roster_resume_native_new_game();
        return;
    }
    if (roster_key(5u, VK_ESCAPE)) {
        roster_waiting_new_game = FALSE;
        menu_open = FALSE;
        roster_pending_controller = NULL;
        menu_texture_dirty = TRUE;
        SudekiMpLogWrite(
            "cleanroom_menu event=native_roster state=closed reason=escape\r\n"
        );
        return;
    }
    if (roster_key(1u, VK_UP) || roster_key(2u, VK_DOWN)) {
        roster_cursor = roster_cursor == 0u ? 1u : 0u;
        menu_texture_dirty = TRUE;
    }
    if (roster_key(6u, VK_LEFT)) {
        *selected = *selected == 0u ? MENU_ACTOR_COUNT - 1u : *selected - 1u;
        menu_texture_dirty = TRUE;
    }
    if (roster_key(7u, VK_RIGHT)) {
        *selected = (*selected + 1u) % MENU_ACTOR_COUNT;
        menu_texture_dirty = TRUE;
    }
    if (roster_key(3u, VK_RETURN)) {
        if (roster_player_one == roster_player_two) {
            SudekiMpLogWrite(
                "cleanroom_menu event=co_op_roster status=rejected "
                "reason=duplicate_character\r\n"
            );
            return;
        }
        if (SudekiMpSplitScreenSetRosterTypes(
                roster_actor_type(roster_player_one),
                roster_actor_type(roster_player_two))) {
            roster_locked = TRUE;
            menu_texture_dirty = TRUE;
            SudekiMpLogFormat(
                "cleanroom_menu event=native_roster status=locked "
                "p1=%s p2=%s policy=save_sidecar_then_native_resume\r\n",
                roster_actor_label(roster_player_one),
                roster_actor_label(roster_player_two)
            );
        }
    }
}

static void __attribute__((thiscall)) cleanroom_controller_update(
    void *controller,
    void *update_data
) {
    original_controller_update(controller, update_data);
    if (roster_native_screen && roster_native_screen_dirty) {
        native_roster_refresh_screen(controller);
        roster_native_screen_dirty = FALSE;
    }
    SudekiMpCleanroomMenuUpdate();
}

static void service_story_test_boost(void) {
    BOOL foreground;
    BOOL key_down;
    BOOL key_rising;
    BOOL world_ready;
    BOOL should_protect;
    BOOL should_apply;
    BOOL speed_ok;
    BOOL party_ok;
    BOOL applied;

    if (!story_test_boost_enabled) {
        return;
    }
    foreground = owns_foreground();
    key_down = (GetAsyncKeyState((int)story_test_boost_key) & 0x8000) != 0;
    key_rising = key_down && !story_test_boost_key_was_down;
    story_test_boost_key_was_down = key_down;
    if (foreground && key_rising) {
        story_test_boost_active = !story_test_boost_active;
        story_test_boost_failure_logged = FALSE;
        SudekiMpLogFormat(
            "story_test_boost event=toggle requested=%s "
            "multiplier_bits=0x%08lx key=0x%02lx\r\n",
            story_test_boost_active ? "on" : "off",
            (unsigned long)float_bits(story_test_boost_multiplier),
            (unsigned long)story_test_boost_key
        );
    }

    world_ready = SudekiMpCleanroomEngineWorldReady();
    should_protect = story_test_boost_active && world_ready;
    should_apply = should_protect && foreground;
    party_ok = SudekiMpCleanroomEngineMaintainPartyInvulnerability(
        should_protect
    );
    speed_ok = SudekiMpCleanroomEngineSetStoryTestSpeed(
        should_apply && party_ok,
        story_test_boost_multiplier
    );
    if (should_apply && party_ok && !speed_ok) {
        BOOL party_rollback_ok;
        BOOL speed_rollback_ok;

        /* A native/cutscene speed owner changed the global, or applying our
         * multiplier failed verification. Yield ownership atomically instead
         * of leaving protection and timing in a half-enabled state. */
        story_test_boost_active = FALSE;
        party_rollback_ok =
            SudekiMpCleanroomEngineMaintainPartyInvulnerability(FALSE);
        speed_rollback_ok = SudekiMpCleanroomEngineSetStoryTestSpeed(
            FALSE,
            story_test_boost_multiplier
        );
        should_protect = FALSE;
        should_apply = FALSE;
        party_ok = party_rollback_ok;
        speed_ok = speed_rollback_ok;
        SudekiMpLogFormat(
            "story_test_boost event=fail_safe status=disabled "
            "party_rollback=%s speed_rollback=%s "
            "reason=speed_apply_or_ownership_conflict\r\n",
            party_rollback_ok ? "confirmed" : "pending",
            speed_rollback_ok ? "confirmed" : "skipped_or_failed"
        );
    }
    applied = should_apply && speed_ok && party_ok;
    if (applied != story_test_boost_runtime_applied) {
        story_test_boost_runtime_applied = applied;
        SudekiMpLogFormat(
            "story_test_boost event=runtime state=%s requested=%s "
            "foreground=%s world_ready=%s speed=%s party=%s "
            "policy=suspend_during_title_loading_or_focus_loss\r\n",
            applied ? "active" : "inactive",
            story_test_boost_active ? "on" : "off",
            foreground ? "true" : "false",
            world_ready ? "true" : "false",
            speed_ok ? "ready" : "failed",
            party_ok ? "ready" : "failed"
        );
    }
    if ((!speed_ok || (should_protect && !party_ok)) &&
        !story_test_boost_failure_logged) {
        story_test_boost_failure_logged = TRUE;
        SudekiMpLogFormat(
            "story_test_boost event=apply status=pending_or_rejected "
            "requested=%s protect_requested=%s speed=%s party=%s "
            "policy=retry_on_game_thread_and_frame_end\r\n",
            story_test_boost_active ? "on" : "off",
            should_protect ? "true" : "false",
            speed_ok ? "ready" : "failed",
            party_ok ? "ready" : "failed"
        );
    } else if (applied) {
        story_test_boost_failure_logged = FALSE;
    }
}

void SudekiMpCleanroomMenuUpdate(void) {
    SudekiMpZoneTransitionVoteSnapshot vote_snapshot;
    SudekiMpSaveBookVoteSnapshot save_book_snapshot;
    BOOL vote_was_active;
    BOOL save_book_vote_was_active;

    if (game_base == NULL) {
        return;
    }
    /* The control-separation freeze path deliberately keeps this update
     * observer alive. Service the save-book owner and consent edges before
     * any frozen-input early return. */
    SudekiMpSaveBookService();
    save_book_vote_was_active = SudekiMpSaveBookGetVoteSnapshot(
        &save_book_snapshot) && save_book_snapshot.active;
    service_save_book_vote_input(
        save_book_vote_was_active ? &save_book_snapshot : NULL);
    /* The custom blacksmith entry suppresses native controller updates while
     * its panels own input. Service it before the generic frozen-input return
     * so keyboard/controller navigation and clean close remain live. */
    SudekiMpBlacksmithUiAdapterService();
    vote_was_active = SudekiMpZoneTransitionGetVoteSnapshot(
        &vote_snapshot) && vote_snapshot.active;
    SudekiMpTalosCoopBalanceService();
    service_story_test_boost();
    clear_roster_resume_guard_after_gameplay_handoff();
    SudekiMpZoneTransitionService();
    if (save_book_vote_was_active || vote_was_active ||
        SudekiMpControlSeparationGameplayInputFrozen()) {
        return;
    }
    if (zone_traversal_mode) {
        SudekiMpZoneTraversalService();
    }
    if (roster_mode) {
        service_loaded_save_coop_autostart();
        poll_roster_input();
        if (roster_locked) {
            SudekiMpSplitScreenApplyRosterOnGameThread();
            {
                BOOL native_roles_locked =
                    SudekiMpSplitScreenRolesLocked();
                if (coop_role_lock_active != native_roles_locked) {
                    /* ApplyRoster owns the atomic control claim and rollback.
                     * This mirror only protects cleanroom UI actions from
                     * mutating an established story roster. */
                    coop_role_lock_active = native_roles_locked;
                    SudekiMpLogFormat(
                        "cleanroom_menu event=native_roster "
                        "state=role_lock_sync locked=%s\r\n",
                        native_roles_locked ? "true" : "false");
                }
            }
        }
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
    static const uint8_t question[7] = {14,17,1,2,4,0,4};

    if (character >= 'A' && character <= 'Z') {
        return font_letters[character - 'A'];
    }
    if (character >= '0' && character <= '9') {
        return font_digits[character - '0'];
    }
    if (character == '>') return greater;
    if (character == '-') return dash;
    if (character == ':') return colon;
    if (character == '?') return question;
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

static unsigned int roster_color_channel(
    unsigned int base,
    unsigned int cyan,
    unsigned int gold,
    unsigned int cyan_weight,
    unsigned int gold_weight
) {
    unsigned int value = base;
    value += cyan * cyan_weight / 255u;
    value += gold * gold_weight / 255u;
    return value > 255u ? 255u : value;
}

/* Return 0..4 covered quarter-pixel samples for a rounded rectangle.  The
 * stock title row is layered: a soft capsule-shaped shadow/rim surrounds a
 * tighter, squarer inset bar.  A shared coverage routine lets both contours
 * remain smooth without baking any original game artwork into the mod. */
static unsigned int roster_rounded_rect_coverage(
    int pixel_x,
    int pixel_y,
    int left,
    int top,
    int right,
    int bottom,
    int radius
) {
    static const int sample_offsets[2] = {1, 3};
    const int center_y_x4 = (top + bottom) * 2;
    int radius_x4;
    int max_radius;
    int cap_left_x4;
    int cap_right_x4;
    int radius_squared;
    unsigned int coverage = 0u;
    int sample_y;

    if (right <= left || bottom <= top || radius <= 0) {
        return 0u;
    }
    max_radius = (bottom - top) / 2;
    if (radius > max_radius) {
        radius = max_radius;
    }
    if (radius > (right - left) / 2) {
        radius = (right - left) / 2;
    }
    radius_x4 = radius * 4;
    cap_left_x4 = left * 4 + radius_x4;
    cap_right_x4 = right * 4 - radius_x4;
    radius_squared = radius_x4 * radius_x4;
    for (sample_y = 0; sample_y < 2; ++sample_y) {
        const int y_x4 = pixel_y * 4 + sample_offsets[sample_y];
        const int delta_y = y_x4 - center_y_x4;
        int sample_x;
        for (sample_x = 0; sample_x < 2; ++sample_x) {
            const int x_x4 = pixel_x * 4 + sample_offsets[sample_x];
            int delta_x = 0;

            if (x_x4 < cap_left_x4) {
                delta_x = x_x4 - cap_left_x4;
            }
            else if (x_x4 > cap_right_x4) {
                delta_x = x_x4 - cap_right_x4;
            }
            if (delta_x * delta_x + delta_y * delta_y <= radius_squared) {
                ++coverage;
            }
        }
    }
    return coverage;
}

static void draw_roster_button_capsule(
    uint32_t *pixels,
    int pitch,
    int top,
    BOOL highlighted
) {
    /* The first prototype copied the near-full-width proportions of the
     * title page.  On an independent roster page that read as an empty rail
     * rather than a button.  Keep the native capsule language but give each
     * choice a deliberate, centered 368-pixel footprint. */
    const int left = 136;
    const int right = 504;
    const int bottom = top + (int)ROSTER_CAPSULE_HEIGHT;
    const int border_inset = 2;
    const int inner_radius = 8;
    int y;
    int first_x;
    int last_x_exclusive;
    int first_y;
    int last_y_exclusive;

    first_x = left - 3;
    last_x_exclusive = right + 3;
    first_y = top - (int)ROSTER_CAPSULE_DRAW_TOP_MARGIN;
    last_y_exclusive = bottom +
        (int)ROSTER_CAPSULE_DRAW_BOTTOM_EXCLUSIVE;
    if (first_x < 0) first_x = 0;
    if (last_x_exclusive > (int)MENU_TEXTURE_WIDTH) {
        last_x_exclusive = MENU_TEXTURE_WIDTH;
    }
    if (first_y < 0) first_y = 0;
    if (last_y_exclusive > (int)MENU_TEXTURE_HEIGHT) {
        last_y_exclusive = MENU_TEXTURE_HEIGHT;
    }

    for (y = first_y; y < last_y_exclusive; ++y) {
        uint32_t *row = (uint32_t *)((uint8_t *)pixels + y * pitch);
        int x;
        for (x = first_x; x < last_x_exclusive; ++x) {
            int local_x = x - left;
            unsigned int shadow_coverage = roster_rounded_rect_coverage(
                x, y, left - 2, top + 1, right + 2, bottom + 4, 16);
            unsigned int outer_coverage = roster_rounded_rect_coverage(
                x, y, left, top, right, bottom, 15);
            unsigned int inner_coverage;
            unsigned int vertical;
            unsigned int cyan_weight = 0u;
            unsigned int gold_weight = 0u;
            unsigned int fill_red;
            unsigned int fill_green;
            unsigned int fill_blue;
            unsigned int red;
            unsigned int green;
            unsigned int blue;
            unsigned int alpha;

            if (outer_coverage == 0u) {
                if (shadow_coverage != 0u) {
                    row[x] = ((82u * shadow_coverage / 4u) << 24) |
                        UINT32_C(0x00050406);
                }
                continue;
            }
            inner_coverage = roster_rounded_rect_coverage(
                x,
                y,
                left + border_inset,
                top + border_inset,
                right - border_inset,
                bottom - border_inset,
                inner_radius);
            vertical = (unsigned int)(bottom - y) * 24u /
                (unsigned int)(bottom - top);
            if (highlighted) {
                if (local_x < 120) {
                    cyan_weight = (unsigned int)(120 - local_x) * 190u / 120u;
                }
                if (local_x > right - left - 121) {
                    gold_weight = (unsigned int)(local_x -
                        (right - left - 121)) * 175u / 120u;
                }
            }
            fill_red = roster_color_channel(29u + vertical, 0u, 65u,
                cyan_weight, gold_weight);
            fill_green = roster_color_channel(27u + vertical, 105u, 49u,
                cyan_weight, gold_weight);
            fill_blue = roster_color_channel(30u + vertical, 118u, 0u,
                cyan_weight, gold_weight);
            if (y < top + 5) {
                fill_red = fill_red + 15u > 255u ? 255u : fill_red + 15u;
                fill_green = fill_green + 15u > 255u ? 255u :
                    fill_green + 15u;
                fill_blue = fill_blue + 15u > 255u ? 255u :
                    fill_blue + 15u;
            }
            else if (y >= bottom - 5) {
                fill_red = fill_red > 8u ? fill_red - 8u : 0u;
                fill_green = fill_green > 8u ? fill_green - 8u : 0u;
                fill_blue = fill_blue > 8u ? fill_blue - 8u : 0u;
            }

            /* Blend the antialiased inner contour against the dark outer
             * shell.  Fully covered inner pixels get the gradient; edge
             * samples keep a softly rounded, two-pixel frame. */
            red = (14u * (4u - inner_coverage) +
                fill_red * inner_coverage) / 4u;
            green = (13u * (4u - inner_coverage) +
                fill_green * inner_coverage) / 4u;
            blue = (15u * (4u - inner_coverage) +
                fill_blue * inner_coverage) / 4u;
            alpha = 244u * outer_coverage / 4u;
            row[x] = (alpha << 24) |
                (red << 16) | (green << 8) | blue;
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

/* The roster cards deliberately own only original presentation: their frames,
 * player-colour rings, and compact character monograms.  Sudeki's actual
 * character portraits remain runtime-owned resources; no extracted or copied
 * game pixels belong in this texture. */
static uint32_t roster_player_selection_color(void) {
    return roster_native_screen_kind == NATIVE_ROSTER_PLAYER_TWO ?
        UINT32_C(0xff58aee8) : UINT32_C(0xffdf6158);
}

static uint32_t roster_actor_card_color(unsigned int actor) {
    static const uint32_t colors[MENU_ACTOR_COUNT] = {
        UINT32_C(0xff6c8fc0), /* Ailish */
        UINT32_C(0xffc96b58), /* Tal */
        UINT32_C(0xffd0a94d), /* Buki */
        UINT32_C(0xff5fa77b)  /* Elco */
    };

    return actor < MENU_ACTOR_COUNT ? colors[actor] : UINT32_C(0xff748092);
}

/* Load Game proves the front-end owns four real portrait anchors named
 * ICON_PORTRAIT1..4 and constructs one UIElementCycleIcon for each.  Reuse
 * that exact widget path: the SQX resource remains owned and decoded by
 * Sudeki, and no opaque texture-table handle ever crosses into mod D3D code. */
static void roster_release_native_portraits(void) {
    unsigned int actor;

    if (game_base != NULL) {
        for (actor = 0u; actor < MENU_ACTOR_COUNT; ++actor) {
            void *icon = roster_native_portrait_icons[actor];
            if (!roster_native_portraits_borrowed &&
                menu_memory_readable(icon, 0x40u) &&
                *(void **)icon ==
                    (void *)(game_base + RVA_NATIVE_CYCLE_ICON_VTABLE) &&
                (*(void ***)icon)[0] ==
                    (void *)(game_base + 0x00080b00u)) {
                NativeCycleIconDestructorFunction destroy =
                    (NativeCycleIconDestructorFunction)(*(void ***)icon)[0];
                (void)destroy(icon, 1u);
            }
            roster_native_portrait_icons[actor] = NULL;
            roster_native_portrait_textures[actor] = NULL;
            roster_native_portrait_gpu_textures[actor] = NULL;
        }
    }
    else {
        ZeroMemory(roster_native_portrait_icons,
            sizeof(roster_native_portrait_icons));
    }
    roster_native_portrait_owner = NULL;
    roster_native_portrait_controller = NULL;
    roster_native_portraits_requested = FALSE;
    roster_native_portraits_borrowed = FALSE;
}

/* UIElementCycleIcon+0x34 is not an opaque portrait token.  On the supported
 * build it is a cSOLMaterial.  Its first resident texture entry owns the
 * Wine/D3D9 texture that Sudeki already decoded from the user's data files:
 *
 *   CycleIcon+0x34 -> cSOLMaterial
 *   material+0x08  -> resident-texture pointer array
 *   array[0]+0x04  -> resident backend
 *   backend+0x04   -> IDirect3DTexture9
 *
 * Keep every owner intact and borrow only the final COM pointer while the
 * native Load Game page remains resident.  This avoids extracting, copying,
 * or redistributing any game-owned portrait asset. */
static void *roster_resolve_native_portrait_gpu_texture(
    void *icon,
    unsigned int actor
) {
    void *material;
    void **texture_array;
    void *resident_texture;
    void *backend;
    void *gpu_texture;
    void **gpu_vtable;

    if (game_base == NULL || !menu_memory_readable(icon, 0x38u)) {
        return NULL;
    }
    material = *(void **)((uint8_t *)icon + 0x34u);
    if (!menu_memory_readable(material, 0x0cu) ||
        *(void **)material != game_base + RVA_SOL_MATERIAL_VTABLE) {
        return NULL;
    }
    texture_array = *(void ***)((uint8_t *)material + 0x08u);
    if (!menu_memory_readable(texture_array, sizeof(void *))) {
        return NULL;
    }
    resident_texture = texture_array[0];
    if (!menu_memory_readable(resident_texture, 0x08u) ||
        *(void **)resident_texture !=
            game_base + RVA_NATIVE_RESIDENT_D3D_TEXTURE_VTABLE) {
        return NULL;
    }
    backend = *(void **)((uint8_t *)resident_texture + 0x04u);
    if (!menu_memory_readable(backend, 0x08u)) {
        return NULL;
    }
    gpu_texture = *(void **)((uint8_t *)backend + 0x04u);
    if (!menu_memory_readable(gpu_texture, sizeof(void *))) {
        return NULL;
    }
    gpu_vtable = *(void ***)gpu_texture;
    if (!menu_memory_readable(gpu_vtable, sizeof(void *)) ||
        !menu_memory_executable(gpu_vtable[0])) {
        return NULL;
    }
    roster_native_portrait_textures[actor] = material;
    SudekiMpLogFormat(
        "cleanroom_menu event=native_roster_portrait phase=gpu_resolve "
        "status=active actor=%s material=%p resident=%p backend=%p gpu=%p "
        "ownership=borrowed_game_resident_texture\r\n",
        roster_display_actor_label(actor), material, resident_texture,
        backend, gpu_texture);
    return gpu_texture;
}

static void roster_hide_native_portrait_anchor(void *icon) {
    void **vtable;

    if (game_base == NULL || !menu_memory_readable(icon, 0x40u)) {
        return;
    }
    vtable = *(void ***)icon;
    if (!menu_memory_readable(vtable, 6u * sizeof(void *)) ||
        vtable[4] != game_base + RVA_NATIVE_CYCLE_ICON_VISIBILITY ||
        !menu_memory_executable(vtable[4])) {
        return;
    }
    ((void (__attribute__((thiscall)) *)(void *, unsigned int))vtable[4])(
        icon, 2u);
}

static void roster_request_native_portraits(void *controller) {
    static const uint8_t constructor_entry[] = {
        0xd9, 0xe8, 0xff, 0x05
    };
    static const uint8_t bind_entry[] = {
        0x81, 0xec, 0x84, 0x00, 0x00, 0x00, 0x53
    };
    static const uint8_t selector_entry[] = {
        0x56, 0x57, 0x50, 0x83, 0xec, 0x0c
    };
    static const char portrait_element_name[] = "ICON_PORTRAIT%d";
    static const char portrait_scene_name[] = "ICON_PORTRAIT%dSG";
    GameAllocateFunction allocate;
    NativeCycleIconConstructorFunction construct;
    void *selector_target;
    void *load_game_page;
    uint8_t *native_icon_array;
    void *options_page;
    void *ui_layer;
    void *owner;
    unsigned int native_resource_indices[MENU_ACTOR_COUNT];
    unsigned int actor;

    if (roster_native_portraits_requested) {
        return;
    }
    if (controller == NULL || game_base == NULL ||
        !menu_memory_readable(controller, 0xb4u)) {
        SudekiMpLogFormat(
            "cleanroom_menu event=native_roster_portrait phase=preflight "
            "status=rejected reason=controller_or_module "
            "controller=%p game_base=%p readable=%d\r\n",
            controller, game_base,
            controller != NULL && menu_memory_readable(controller, 0xb4u));
        goto rejected;
    }
    selector_target = game_base + RVA_HUD_PORTRAIT_RESOURCE_SELECT;
    if (memcmp(game_base + RVA_NATIVE_CYCLE_ICON_CONSTRUCTOR,
            constructor_entry, sizeof(constructor_entry)) != 0 ||
        memcmp(game_base + RVA_NATIVE_CYCLE_ICON_BIND,
            bind_entry, sizeof(bind_entry)) != 0 ||
        memcmp(selector_target,
            selector_entry, sizeof(selector_entry)) != 0 ||
        !menu_memory_executable(
            game_base + RVA_NATIVE_CYCLE_ICON_CONSTRUCTOR) ||
        !menu_memory_executable(game_base + RVA_NATIVE_CYCLE_ICON_BIND) ||
        !menu_memory_executable(
            game_base + RVA_HUD_PORTRAIT_RESOURCE_SELECT) ||
        !menu_memory_readable(game_base +
            RVA_HUD_PORTRAIT_RESOURCE_INDEX_TABLE,
            16u * sizeof(unsigned int))) {
        SudekiMpLogFormat(
            "cleanroom_menu event=native_roster_portrait phase=preflight "
            "status=rejected reason=exact_image_gate constructor_bytes=%d "
            "bind_bytes=%d selector_bytes=%d constructor_exec=%d "
            "bind_exec=%d selector_exec=%d table_readable=%d\r\n",
            memcmp(game_base + RVA_NATIVE_CYCLE_ICON_CONSTRUCTOR,
                constructor_entry, sizeof(constructor_entry)) == 0,
            memcmp(game_base + RVA_NATIVE_CYCLE_ICON_BIND,
                bind_entry, sizeof(bind_entry)) == 0,
            memcmp(selector_target,
                selector_entry, sizeof(selector_entry)) == 0,
            menu_memory_executable(
                game_base + RVA_NATIVE_CYCLE_ICON_CONSTRUCTOR),
            menu_memory_executable(game_base + RVA_NATIVE_CYCLE_ICON_BIND),
            menu_memory_executable(
                game_base + RVA_HUD_PORTRAIT_RESOURCE_SELECT),
            menu_memory_readable(game_base +
                RVA_HUD_PORTRAIT_RESOURCE_INDEX_TABLE,
                16u * sizeof(unsigned int)));
        goto rejected;
    }
    /* The Load Game page owns the exact four portrait widgets we need.  Its
     * initialization allocates a contiguous UIElementCycleIcon[4] array at
     * page+0xA4 and binds those elements against the page's private
     * ICON_PORTRAIT1..4 scene owner.  Borrow those widgets instead of trying
     * to reproduce their unexported owner from the Options scene. */
    load_game_page = *(void **)((uint8_t *)controller + 0xb4u);
    if (menu_memory_readable(load_game_page, 0xa8u) &&
        *(void **)load_game_page == game_base + RVA_LOAD_GAME_MENU_VTABLE) {
        native_icon_array = *(uint8_t **)((uint8_t *)load_game_page + 0xa4u);
        if (!menu_memory_readable(native_icon_array,
                MENU_ACTOR_COUNT * 0x40u)) {
            if (!roster_native_portrait_request_failed) {
                roster_native_portrait_request_failed = TRUE;
                SudekiMpLogFormat(
                    "cleanroom_menu event=native_roster_portrait "
                    "phase=native_array status=pending page=%p array=%p "
                    "reason=Load_Game_widgets_not_initialized\r\n",
                    load_game_page, native_icon_array);
            }
            return;
        }
        owner = NULL;
        for (actor = 0u; actor < MENU_ACTOR_COUNT; ++actor) {
            void *icon = native_icon_array + actor * 0x40u;
            void **vtable;
            SudekiMpCleanroomActor engine_actor = roster_display_actor(actor);
            unsigned int portrait_enum = native_portrait_enum_bridge(
                roster_actor_type((unsigned int)engine_actor),
                game_base + RVA_CHARACTER_TYPE_TO_PORTRAIT_ENUM);

            if (*(void **)icon !=
                    (void *)(game_base + RVA_NATIVE_CYCLE_ICON_VTABLE) ||
                *(void **)((uint8_t *)icon + 0x28u) !=
                    (void *)(game_base +
                        RVA_NATIVE_CYCLE_ICON_SECONDARY_VTABLE) ||
                *(uint16_t *)((uint8_t *)icon + 0x2cu) == UINT16_MAX ||
                !menu_memory_readable(
                    *(void **)((uint8_t *)icon + 0x34u), sizeof(void *)) ||
                portrait_enum >= 16u) {
                if (!roster_native_portrait_request_failed) {
                    roster_native_portrait_request_failed = TRUE;
                    SudekiMpLogFormat(
                        "cleanroom_menu event=native_roster_portrait "
                        "phase=native_array status=pending actor=%s icon=%p "
                        "anchor_id=%u anchor=%p reason=widget_not_ready\r\n",
                        roster_display_actor_label(actor), icon,
                        (unsigned int)*(uint16_t *)((uint8_t *)icon + 0x2cu),
                        *(void **)((uint8_t *)icon + 0x34u));
                }
                return;
            }
            vtable = *(void ***)icon;
            if (!menu_memory_readable(vtable, 6u * sizeof(void *)) ||
                vtable[5] != game_base + RVA_NATIVE_CYCLE_ICON_REFRESH ||
                !menu_memory_executable(vtable[5])) {
                return;
            }
            if (owner == NULL) {
                owner = *(void **)((uint8_t *)icon + 0x30u);
            }
            else if (owner != *(void **)((uint8_t *)icon + 0x30u)) {
                return;
            }
            native_resource_indices[actor] = *(unsigned int *)(game_base +
                RVA_HUD_PORTRAIT_RESOURCE_INDEX_TABLE +
                portrait_enum * sizeof(unsigned int));
        }

        roster_native_portrait_owner = owner;
        roster_native_portrait_controller = controller;
        roster_native_portraits_borrowed = TRUE;
        roster_native_portraits_requested = TRUE;
        for (actor = 0u; actor < MENU_ACTOR_COUNT; ++actor) {
            void *icon = native_icon_array + actor * 0x40u;
            void **vtable = *(void ***)icon;

            roster_native_portrait_icons[actor] = icon;
            roster_native_portrait_textures[actor] = NULL;
            roster_native_portrait_gpu_textures[actor] = NULL;
            *(unsigned int *)((uint8_t *)icon + 0x1cu) = 0u;
            native_cycle_icon_assign_bridge(icon,
                native_resource_indices[actor], selector_target);
            ((void (__attribute__((thiscall)) *)(void *))vtable[5])(icon);
            roster_native_portrait_gpu_textures[actor] =
                roster_resolve_native_portrait_gpu_texture(icon, actor);
            if (roster_native_portrait_gpu_textures[actor] == NULL) {
                roster_native_portrait_request_failed = TRUE;
                roster_native_portraits_requested = FALSE;
                SudekiMpLogFormat(
                    "cleanroom_menu event=native_roster_portrait "
                    "phase=gpu_resolve status=pending actor=%s "
                    "reason=resident_texture_not_ready\r\n",
                    roster_display_actor_label(actor));
                return;
            }
            roster_hide_native_portrait_anchor(icon);
            SudekiMpLogFormat(
                "cleanroom_menu event=native_roster_portrait status=active "
                "actor=%s icon=%p anchor_id=%u anchor=%p "
                "resource_index=0x%03lx ownership=borrowed_Load_Game_widget "
                "policy=native_cycle_icon_no_extract\r\n",
                roster_display_actor_label(actor), icon,
                (unsigned int)*(uint16_t *)((uint8_t *)icon + 0x2cu),
                *(void **)((uint8_t *)icon + 0x34u),
                (unsigned long)native_resource_indices[actor]);
        }
        roster_native_portrait_request_failed = FALSE;
        return;
    }
    options_page = *(void **)((uint8_t *)controller + 0xb0u);
    if (!menu_memory_readable(options_page, 0x5cu) ||
        *(void **)options_page != game_base + RVA_OPTIONS_MENU_VTABLE) {
        SudekiMpLogFormat(
            "cleanroom_menu event=native_roster_portrait phase=preflight "
            "status=rejected reason=options_page_gate options_page=%p "
            "readable=%d actual_vtable=%p expected_vtable=%p\r\n",
            options_page, menu_memory_readable(options_page, 0x5cu),
            menu_memory_readable(options_page, sizeof(void *)) ?
                *(void **)options_page : NULL,
            game_base + RVA_OPTIONS_MENU_VTABLE);
        goto rejected;
    }
    ui_layer = *(void **)((uint8_t *)options_page + 0x58u);
    if (!menu_memory_readable(ui_layer, 0xc0u)) {
        SudekiMpLogFormat(
            "cleanroom_menu event=native_roster_portrait phase=preflight "
            "status=rejected reason=ui_layer_gate options_page=%p "
            "ui_layer=%p readable=%d\r\n",
            options_page, ui_layer, menu_memory_readable(ui_layer, 0xc0u));
        goto rejected;
    }
    owner = *(void **)((uint8_t *)ui_layer + 0xbcu);
    if (!menu_memory_readable(owner, 0x1cu)) {
        SudekiMpLogFormat(
            "cleanroom_menu event=native_roster_portrait phase=preflight "
            "status=rejected reason=owner_gate ui_layer=%p owner=%p "
            "readable=%d\r\n",
            ui_layer, owner, menu_memory_readable(owner, 0x1cu));
        goto rejected;
    }
    SudekiMpLogFormat(
        "cleanroom_menu event=native_roster_portrait phase=owner_gate "
        "controller=%p options_page=%p ui_layer=%p owner=%p\r\n",
        controller, options_page, ui_layer, owner);
    allocate = (GameAllocateFunction)(game_base + RVA_GAME_ALLOCATE);
    construct = (NativeCycleIconConstructorFunction)(
        game_base + RVA_NATIVE_CYCLE_ICON_CONSTRUCTOR);
    roster_native_portraits_borrowed = FALSE;
    roster_native_portraits_requested = TRUE;
    roster_native_portrait_owner = owner;
    roster_native_portrait_controller = controller;

    for (actor = 0u; actor < MENU_ACTOR_COUNT; ++actor) {
        SudekiMpCleanroomActor engine_actor = roster_display_actor(actor);
        unsigned int character_type = roster_actor_type(
            (unsigned int)engine_actor);
        unsigned int portrait_enum;
        unsigned int resource_index;
        void *icon = allocate(0x40u);
        void **vtable;

        if (icon == NULL) {
            goto rejected_after_allocation;
        }
        icon = construct(icon);
        roster_native_portrait_icons[actor] = icon;
        if (!menu_memory_readable(icon, 0x40u) ||
            *(void **)icon !=
                (void *)(game_base + RVA_NATIVE_CYCLE_ICON_VTABLE) ||
            *(void **)((uint8_t *)icon + 0x28u) !=
                (void *)(game_base + RVA_NATIVE_CYCLE_ICON_SECONDARY_VTABLE)) {
            goto rejected_after_allocation;
        }
        native_cycle_icon_bind_bridge(
            icon, portrait_element_name, portrait_scene_name, actor + 1u,
            owner, game_base + RVA_NATIVE_CYCLE_ICON_BIND);
        SudekiMpLogFormat(
            "cleanroom_menu event=native_roster_portrait phase=bind_probe "
            "actor=%s icon=%p expected_owner=%p actual_owner=%p "
            "anchor_id=%u anchor=%p\r\n",
            roster_display_actor_label(actor), icon, owner,
            *(void **)((uint8_t *)icon + 0x30u),
            (unsigned int)*(uint16_t *)((uint8_t *)icon + 0x2cu),
            *(void **)((uint8_t *)icon + 0x34u));
        if (*(void **)((uint8_t *)icon + 0x30u) != owner ||
            *(uint16_t *)((uint8_t *)icon + 0x2cu) == UINT16_MAX ||
            !menu_memory_readable(
                *(void **)((uint8_t *)icon + 0x34u), sizeof(void *))) {
            goto rejected_after_allocation;
        }
        portrait_enum = native_portrait_enum_bridge(
            character_type,
            game_base + RVA_CHARACTER_TYPE_TO_PORTRAIT_ENUM);
        if (portrait_enum >= 16u) {
            goto rejected_after_allocation;
        }
        resource_index = *(unsigned int *)(game_base +
            RVA_HUD_PORTRAIT_RESOURCE_INDEX_TABLE +
            portrait_enum * sizeof(resource_index));
        SudekiMpLogFormat(
            "cleanroom_menu event=native_roster_portrait phase=resource_probe "
            "actor=%s character_type=0x%02lx portrait_enum=%lu "
            "resource_index=0x%03lx\r\n",
            roster_display_actor_label(actor),
            (unsigned long)character_type, (unsigned long)portrait_enum,
            (unsigned long)resource_index);
        *(unsigned int *)((uint8_t *)icon + 0x1cu) = 0u;
        native_cycle_icon_assign_bridge(
            icon, resource_index,
            selector_target);
        vtable = *(void ***)icon;
        if (!menu_memory_readable(vtable, 6u * sizeof(void *)) ||
            vtable[5] != game_base + RVA_NATIVE_CYCLE_ICON_REFRESH ||
            !menu_memory_executable(vtable[5])) {
            goto rejected_after_allocation;
        }
        ((void (__attribute__((thiscall)) *)(void *))vtable[5])(icon);
        roster_native_portrait_gpu_textures[actor] =
            roster_resolve_native_portrait_gpu_texture(icon, actor);
        if (roster_native_portrait_gpu_textures[actor] == NULL) {
            goto rejected_after_allocation;
        }
        roster_hide_native_portrait_anchor(icon);
        SudekiMpLogFormat(
            "cleanroom_menu event=native_roster_portrait status=active "
            "actor=%s icon=%p anchor_id=%u anchor=%p portrait_enum=%lu "
            "resource_index=0x%03lx policy=native_cycle_icon_no_extract\r\n",
            roster_display_actor_label(actor), icon,
            (unsigned int)*(uint16_t *)((uint8_t *)icon + 0x2cu),
            *(void **)((uint8_t *)icon + 0x34u),
            (unsigned long)portrait_enum, (unsigned long)resource_index);
    }
    roster_native_portrait_request_failed = FALSE;
    return;

rejected_after_allocation:
    roster_release_native_portraits();
rejected:
    if (!roster_native_portrait_request_failed) {
        roster_native_portrait_request_failed = TRUE;
        SudekiMpLogWrite(
            "cleanroom_menu event=native_roster_portrait status=rejected "
            "reason=native_cycle_icon_owner_anchor_or_resource_gate "
            "fallback=original_card_silhouette\r\n");
    }
}

static void draw_roster_character_card(
    uint32_t *pixels,
    int pitch,
    unsigned int actor,
    BOOL selected,
    BOOL unavailable
) {
    const int left = 40 + (int)actor * 144;
    const int right = left + 126;
    const int top = 306;
    const int bottom = 389;
    const uint32_t actor_color = roster_actor_card_color(actor);
    const uint32_t selection_color = roster_player_selection_color();
    const char *label = roster_display_actor_label(actor);
    int y;

    if (selected) {
        /* A broad translucent glow makes ownership readable before the
         * player needs to inspect the name or portrait. */
        fill_rectangle(pixels, pitch,
            left - 5, top - 5, right + 5, bottom + 5,
            (selection_color & UINT32_C(0x00ffffff)) |
                UINT32_C(0x4f000000));
    }

    for (y = top - 3; y < bottom + 4; ++y) {
        uint32_t *row = (uint32_t *)((uint8_t *)pixels + y * pitch);
        int x;
        for (x = left - 3; x < right + 4; ++x) {
            const unsigned int outer = roster_rounded_rect_coverage(
                x, y, left, top, right, bottom, 13);
            const unsigned int inner = roster_rounded_rect_coverage(
                x, y, left + 3, top + 3, right - 3, bottom - 3, 10);
            const unsigned int shadow = roster_rounded_rect_coverage(
                x, y, left + 2, top + 4, right + 3, bottom + 4, 13);
            uint32_t fill;

            if (outer == 0u) {
                if (shadow != 0u) {
                    row[x] = ((66u * shadow / 4u) << 24) | UINT32_C(0x00020305);
                }
                continue;
            }
            if (inner == 0u) {
                const uint32_t ring = selected ? selection_color : actor_color;
                const unsigned int ring_alpha = selected ? 244u : 146u;
                row[x] = ((ring_alpha * outer / 4u) << 24) |
                    (ring & UINT32_C(0x00ffffff));
                continue;
            }
            fill = unavailable ? UINT32_C(0xff20252e) :
                (selected ? UINT32_C(0xff243245) :
                    UINT32_C(0xff192331));
            if (selected && (x < left + 5 || x > right - 6 ||
                    y < top + 5 || y > bottom - 6)) {
                fill = (fill & UINT32_C(0xff1f1f1f)) |
                    (selection_color & UINT32_C(0x00181818));
            }
            row[x] = ((232u * inner / 4u) << 24) | (fill & UINT32_C(0x00ffffff));
        }
    }

    /* Character identity owns the lower accent; controller ownership owns
     * the small P1/P2 badge.  This keeps Ailish blue, Tal red, Buki gold and
     * Elco green without confusing those colors with player numbers. */
    fill_rectangle(pixels, pitch,
        left + 10, bottom - 5, right - 10, bottom - 2, actor_color);
    if (selected) {
        const char *player_badge = roster_native_screen_kind ==
            NATIVE_ROSTER_PLAYER_TWO ? "P2" : "P1";
        fill_rectangle(pixels, pitch,
            left + 8, top - 13, left + 38, top + 2, selection_color);
        draw_text(pixels, pitch, left + 17, top - 9,
            player_badge, UINT32_C(0xffffffff), 1);
    }

    /* The silhouette is only a safe fallback while Sudeki's own portrait is
     * unavailable.  Once the resident game texture resolves, leave the card
     * interior empty so the head is the sole portrait presentation. */
    if (roster_native_portrait_gpu_textures[actor] == NULL || unavailable) {
        fill_rectangle(pixels, pitch,
            left + 49, top + 17, left + 77, top + 43, actor_color);
        fill_rectangle(pixels, pitch,
            left + 42, top + 43, left + 84, top + 63, actor_color);
        fill_rectangle(pixels, pitch,
            left + 46, top + 13, left + 80, top + 18,
            selected ? selection_color : UINT32_C(0xffc8d1dc));
        if (unavailable) {
            int offset;
            for (offset = 0; offset < 48; ++offset) {
                fill_rectangle(pixels, pitch,
                    left + 39 + offset, top + 15 + offset,
                    left + 41 + offset, top + 17 + offset,
                    UINT32_C(0xd8b9c0c8));
                fill_rectangle(pixels, pitch,
                    right - 41 - offset, top + 15 + offset,
                    right - 39 - offset, top + 17 + offset,
                    UINT32_C(0xd8b9c0c8));
            }
            draw_text(pixels, pitch, left + 48, top + 67,
                "TAKEN", UINT32_C(0xffb9c0c8), 1);
        }
        else {
            draw_text(pixels, pitch, left + 56, top + 66,
                label[0] == '\0' ? "?" : (char[2]){label[0], '\0'},
                selected ? UINT32_C(0xffffffff) :
                    UINT32_C(0xffb6c0cc), 2);
        }
    }
}

static void draw_roster_character_back_button(
    uint32_t *pixels,
    int pitch,
    BOOL selected
) {
    const int left = 238;
    const int right = 402;
    const int top = 407;
    const int bottom = 435;
    const uint32_t accent = selected ? roster_player_selection_color() :
        UINT32_C(0xff465366);
    int y;

    for (y = top - 2; y < bottom + 3; ++y) {
        uint32_t *row = (uint32_t *)((uint8_t *)pixels + y * pitch);
        int x;
        for (x = left - 2; x < right + 3; ++x) {
            const unsigned int outer = roster_rounded_rect_coverage(
                x, y, left, top, right, bottom, 14);
            const unsigned int inner = roster_rounded_rect_coverage(
                x, y, left + 3, top + 3, right - 3, bottom - 3, 10);

            if (outer == 0u) {
                continue;
            }
            if (inner == 0u) {
                row[x] = ((selected ? 242u : 170u) * outer / 4u << 24) |
                    (accent & UINT32_C(0x00ffffff));
            }
            else {
                const uint32_t fill = selected ? UINT32_C(0xff26384a) :
                    UINT32_C(0xff18212d);
                row[x] = (232u * inner / 4u << 24) |
                    (fill & UINT32_C(0x00ffffff));
            }
        }
    }
}

static void draw_roster_character_cards(uint32_t *pixels, int pitch) {
    unsigned int actor;
    const BOOL player_two = roster_native_screen_kind ==
        NATIVE_ROSTER_PLAYER_TWO;

    if (roster_native_screen_kind != NATIVE_ROSTER_PLAYER_ONE &&
        !player_two) {
        return;
    }
    for (actor = 0u; actor < MENU_ACTOR_COUNT; ++actor) {
        draw_roster_character_card(
            pixels,
            pitch,
            actor,
            actor == roster_native_selection,
            player_two && roster_display_actor(actor) == roster_player_one
        );
    }
    draw_roster_character_back_button(
        pixels, pitch, roster_native_selection == 4u);
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
    if (zone_traversal_mode) {
        const char *current_world = SudekiMpZoneTraversalCurrentWorld();
        unsigned int index;
        unsigned int first_index = 0u;
        unsigned int interior_count = traversal_interior_count_for_world(
            current_world, &first_index);

        draw_text(
            pixels, locked.pitch, 32, 24, "SUDEKIMP WORLD TRAVEL",
            UINT32_C(0xff5ef7f0), 3);
        draw_text(
            pixels, locked.pitch, 32, 56,
            "F7 CLOSE  UP DOWN CHOOSE  RIGHT AREAS  LEFT WORLDS",
            UINT32_C(0xffaab8c8), 2);
        draw_text(
            pixels, locked.pitch, 32, 82,
            current_world == NULL ? "WORLD: UNKNOWN" : "WORLD:",
            UINT32_C(0xffb9c4d1), 2);
        if (current_world != NULL) {
            draw_text(pixels, locked.pitch, 190, 82, current_world,
                UINT32_C(0xff7cf29a), 2);
        }
        if (zone_traversal_waiting) {
            draw_text(pixels, locked.pitch, 32, 108,
                "WAITING FOR WORLD LOAD",
                UINT32_C(0xffffd166), 2);
        }
        if (zone_traversal_page == ZONE_TRAVERSAL_PAGE_WORLDS) {
            draw_text(pixels, locked.pitch, 32, 140,
                "PERSISTENT WORLDS - ENTER USES DEFAULT START",
                UINT32_C(0xffd6dce5), 2);
            for (index = 0u; index < sizeof(traversal_worlds) /
                    sizeof(traversal_worlds[0]); ++index) {
                int y = 174 + (int)index * 38;
                if (index == zone_traversal_selection) {
                    fill_rectangle(pixels, locked.pitch, 20, y - 8,
                        MENU_TEXTURE_WIDTH - 20, y + 22,
                        UINT32_C(0x90324962));
                    draw_text(pixels, locked.pitch, 30, y, ">",
                        UINT32_C(0xffffffff), 2);
                }
                draw_text(pixels, locked.pitch, 58, y,
                    traversal_worlds[index].label,
                    UINT32_C(0xffffffff), 2);
            }
        }
        else {
            draw_text(pixels, locked.pitch, 32, 140,
                "TEMPORARY AREAS - ACTIVE WORLD ONLY",
                UINT32_C(0xffd6dce5), 2);
            if (interior_count == 0u) {
                draw_text(pixels, locked.pitch, 58, 184,
                    "NO CONFIRMED AREAS FOR THIS WORLD",
                    UINT32_C(0xffff6b6b), 2);
            }
            else {
                for (index = 0u; index < interior_count; ++index) {
                    int y = 174 + (int)index * 38;
                    if (index == zone_traversal_selection) {
                        fill_rectangle(pixels, locked.pitch, 20, y - 8,
                            MENU_TEXTURE_WIDTH - 20, y + 22,
                            UINT32_C(0x90324962));
                        draw_text(pixels, locked.pitch, 30, y, ">",
                            UINT32_C(0xffffffff), 2);
                    }
                    draw_text(pixels, locked.pitch, 58, y,
                        traversal_interiors[first_index + index].label,
                        UINT32_C(0xffffffff), 2);
                }
            }
        }
        result = unlock_rectangle(texture, 0u);
        if (FAILED(result)) {
            return FALSE;
        }
        menu_texture_dirty = FALSE;
        return TRUE;
    }
    if (roster_mode) {
        draw_text(
            pixels, locked.pitch, 32, 24, "SUDEKIMP CO-OP ROSTER",
            UINT32_C(0xff5ef7f0), 3);
        draw_text(
            pixels, locked.pitch, 32, 58,
            "UP DOWN PLAYER  LEFT RIGHT CHOOSE  ENTER LOCK",
            UINT32_C(0xffaab8c8), 2);
        draw_text(
            pixels, locked.pitch, 72, 150,
            roster_cursor == 0u ? "> P1" : "  P1",
            UINT32_C(0xffffffff), 3);
        fill_rectangle(
            pixels, locked.pitch, 178, 136, 232, 190,
            UINT32_C(0xff273b56));
        draw_text(
            pixels, locked.pitch, 194, 150,
            roster_actor_label(roster_player_one)[0] == '\0' ? "?" :
                (char[2]){roster_actor_label(roster_player_one)[0], '\0'},
            UINT32_C(0xffffffff), 3);
        draw_text(
            pixels, locked.pitch, 250, 150,
            roster_actor_label(roster_player_one),
            UINT32_C(0xff7cf29a), 3);
        draw_text(
            pixels, locked.pitch, 72, 250,
            roster_cursor == 1u ? "> P2" : "  P2",
            UINT32_C(0xffffffff), 3);
        fill_rectangle(
            pixels, locked.pitch, 178, 236, 232, 290,
            UINT32_C(0xff493b25));
        draw_text(
            pixels, locked.pitch, 194, 250,
            roster_actor_label(roster_player_two)[0] == '\0' ? "?" :
                (char[2]){roster_actor_label(roster_player_two)[0], '\0'},
            UINT32_C(0xffffffff), 3);
        draw_text(
            pixels, locked.pitch, 250, 250,
            roster_actor_label(roster_player_two),
            UINT32_C(0xffffd166), 3);
        draw_text(
            pixels, locked.pitch, 72, 370,
            "ROLES LOCK BEFORE GAMEPLAY",
            UINT32_C(0xffb9c4d1), 2);
        result = unlock_rectangle(texture, 0u);
        if (FAILED(result)) {
            return FALSE;
        }
        menu_texture_dirty = FALSE;
        return TRUE;
    }
    draw_text(
        pixels, locked.pitch, 32, 24,
        integrated_multiplayer_mode && !coop_role_lock_active ?
            "SUDEKIMP CO-OP ROLE LOBBY" : "SUDEKIMP CLEANROOM",
        UINT32_C(0xff5ef7f0), 3);
    draw_text(
        pixels, locked.pitch, 32, 56,
        "F8 OR ESC CLOSE  ARROWS AND ENTER TOGGLE",
        UINT32_C(0xffaab8c8), 2);

    for (index = 0u; index < MENU_ITEM_COUNT; ++index) {
        int y = 88 + (int)index * 30;
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
            } else if (!coop_role_lock_active) {
                status = "USE CO-OP READY";
                status_color = UINT32_C(0xffffd166);
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
        } else if (index == MENU_INFINITE_JETPACK_INDEX) {
            label = "INFINITE JETPACK";
            status = infinite_jetpack_fuel_valid ?
                (infinite_jetpack_fuel ? "ENABLED" : "DISABLED") :
                "UNAVAILABLE";
            if (infinite_jetpack_fuel_valid && infinite_jetpack_fuel) {
                status_color = UINT32_C(0xff7cf29a);
            }
        } else if (index == MENU_COOP_READY_INDEX) {
            label = "CO-OP READY";
            if (!integrated_multiplayer_mode) {
                status = "UNAVAILABLE";
            } else if (coop_role_lock_active) {
                status = "ROLES LOCKED";
                status_color = UINT32_C(0xff7cf29a);
            } else if (coop_selected_actor != SUDEKIMP_CLEANROOM_AILISH &&
                       item_present[coop_selected_actor]) {
                status = SudekiMpCleanroomActorLabel(coop_selected_actor);
                status_color = UINT32_C(0xffffd166);
            } else {
                status = "CHOOSE P2 FIRST";
                status_color = UINT32_C(0xffffd166);
            }
            if (coop_ready_failed_until != 0u &&
                (LONG)(coop_ready_failed_until - GetTickCount()) > 0) {
                status = "REJECTED";
                status_color = UINT32_C(0xffff6b6b);
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

static BOOL update_roster_button_texture(void *texture) {
    void **vtable = *(void ***)texture;
    D3DTextureLockRectFunction lock_rectangle;
    D3DTextureUnlockRectFunction unlock_rectangle;
    SudekiMpD3DLockedRect locked;
    uint32_t *pixels;
    unsigned int count;
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
    if (FAILED(result)) {
        return FALSE;
    }
    if (locked.bits == NULL ||
        locked.pitch < (int)(MENU_TEXTURE_WIDTH * sizeof(uint32_t))) {
        (void)unlock_rectangle(texture, 0u);
        return FALSE;
    }
    pixels = (uint32_t *)locked.bits;
    fill_rectangle(pixels, locked.pitch, 0, 0,
        MENU_TEXTURE_WIDTH, MENU_TEXTURE_HEIGHT, UINT32_C(0x00000000));
    if (roster_native_screen_kind == NATIVE_ROSTER_PLAYER_ONE ||
        roster_native_screen_kind == NATIVE_ROSTER_PLAYER_TWO) {
        draw_roster_character_cards(pixels, locked.pitch);
        result = unlock_rectangle(texture, 0u);
        if (FAILED(result)) {
            return FALSE;
        }
        roster_button_texture_dirty = FALSE;
        return TRUE;
    }
    count = native_roster_item_count();
    if (count > NATIVE_TITLE_ROW_COUNT) {
        count = NATIVE_TITLE_ROW_COUNT;
    }
    for (index = 0u; index < count; ++index) {
        draw_roster_button_capsule(
            pixels,
            locked.pitch,
            ROSTER_CAPSULE_FIRST_TOP +
                (int)index * ROSTER_CAPSULE_ROW_SPACING,
            index == roster_native_selection);
    }
    result = unlock_rectangle(texture, 0u);
    if (FAILED(result)) {
        return FALSE;
    }
    roster_button_texture_dirty = FALSE;
    return TRUE;
}

static BOOL ensure_roster_button_texture(void *device) {
    void **vtable;
    D3DCreateTextureFunction create_texture;
    HRESULT result;

    if (roster_button_texture_device != device) {
        release_com_object(&roster_button_texture);
        roster_button_texture_device = device;
        roster_button_texture_dirty = TRUE;
    }
    if (roster_button_texture != NULL) {
        return !roster_button_texture_dirty ||
            update_roster_button_texture(roster_button_texture);
    }
    vtable = *(void ***)device;
    create_texture = vtable == NULL ? NULL : (D3DCreateTextureFunction)
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
        &roster_button_texture,
        NULL);
    if (FAILED(result) || roster_button_texture == NULL) {
        roster_button_texture = NULL;
        return FALSE;
    }
    roster_button_texture_dirty = TRUE;
    return update_roster_button_texture(roster_button_texture);
}

static BOOL update_roster_backdrop_texture(void *texture) {
    void **vtable = *(void ***)texture;
    D3DTextureLockRectFunction lock_rectangle;
    D3DTextureUnlockRectFunction unlock_rectangle;
    SudekiMpD3DLockedRect locked;
    unsigned int y;
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
    /* A restrained opaque slate gradient replaces both native backing pages.
     * It is original SudekiMP presentation, not a copied game asset. */
    for (y = 0u; y < ROSTER_BACKDROP_HEIGHT; ++y) {
        uint32_t *row = (uint32_t *)((uint8_t *)locked.bits + y * locked.pitch);
        unsigned int x;
        const unsigned int vertical = y * 18u /
            (ROSTER_BACKDROP_HEIGHT - 1u);

        for (x = 0u; x < ROSTER_BACKDROP_WIDTH; ++x) {
            const unsigned int center = x < ROSTER_BACKDROP_WIDTH / 2u ?
                x : ROSTER_BACKDROP_WIDTH - 1u - x;
            const unsigned int glow = center * 9u /
                (ROSTER_BACKDROP_WIDTH / 2u);
            const unsigned int red = 13u + glow / 3u + vertical / 4u;
            const unsigned int green = 19u + glow / 2u + vertical / 3u;
            const unsigned int blue = 29u + glow + vertical / 2u;

            row[x] = UINT32_C(0xff000000) |
                (red << 16) | (green << 8) | blue;
        }
    }
    result = unlock_rectangle(texture, 0u);
    return SUCCEEDED(result);
}

static BOOL ensure_roster_backdrop_texture(void *device) {
    void **vtable;
    D3DCreateTextureFunction create_texture;
    HRESULT result;

    if (roster_backdrop_texture_device != device) {
        release_com_object(&roster_backdrop_texture);
        roster_backdrop_texture_device = device;
    }
    if (roster_backdrop_texture != NULL) {
        return TRUE;
    }
    vtable = *(void ***)device;
    create_texture = vtable == NULL ? NULL : (D3DCreateTextureFunction)
        vtable[D3D_DEVICE_CREATE_TEXTURE_INDEX];
    if (create_texture == NULL) {
        return FALSE;
    }
    result = create_texture(
        device,
        ROSTER_BACKDROP_WIDTH,
        ROSTER_BACKDROP_HEIGHT,
        1u,
        D3D_USAGE_DYNAMIC,
        D3D_FORMAT_A8R8G8B8,
        D3D_POOL_DEFAULT,
        &roster_backdrop_texture,
        NULL);
    if (FAILED(result) || roster_backdrop_texture == NULL ||
        !update_roster_backdrop_texture(roster_backdrop_texture)) {
        release_com_object(&roster_backdrop_texture);
        return FALSE;
    }
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
    BOOL participation_requested;
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
    participation_requested =
        SudekiMpSplitScreenRosterParticipationRequested();
    /* A generic interaction request has no proven target at this layer.
     * Present only participation/input readiness here; modal action feedback
     * belongs to the owner that can actually consume the routed A/B intent. */
    accent = !participation_requested ? UINT32_C(0xff58aee8) :
        (multiplayer_active && multiplayer_input_ready ?
            UINT32_C(0xff7cf29a) : UINT32_C(0xffffd166));
    text = !participation_requested ? "P2 START JOIN" :
        (multiplayer_active && multiplayer_input_ready ?
            "P2 READY" :
            (multiplayer_active ? "P2 CONNECT" : "P2 JOINING"));
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
        pixels, locked.pitch,
        12,
        13,
        26,
        29,
        accent);
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

static BOOL draw_blacksmith_ui_presentation(
    uint32_t *pixels,
    int pitch,
    const SudekiMpBlacksmithUiPresentation *presentation
) {
    uint32_t player_index;

    if (pixels == NULL || pitch <= 0 || presentation == NULL) {
        return FALSE;
    }
    for (player_index = 0u;
         player_index < SUDEKIMP_BLACKSMITH_UI_PLAYER_COUNT;
         ++player_index) {
        const SudekiMpBlacksmithUiViewportPresentation *viewport =
            &presentation->viewports[player_index];
        const int viewport_left =
            (int)(player_index * SUDEKIMP_BLACKSMITH_UI_VIEWPORT_WIDTH);
        uint32_t command_index;

        for (command_index = 0u;
             command_index < viewport->command_count;
             ++command_index) {
            const SudekiMpBlacksmithUiDrawCommand *command =
                &viewport->commands[command_index];

            if (command->kind == SUDEKIMP_BLACKSMITH_UI_DRAW_RECTANGLE) {
                fill_rectangle(
                    pixels,
                    pitch,
                    viewport_left + command->left,
                    command->top,
                    viewport_left + command->right,
                    command->bottom,
                    command->color);
            } else if (command->kind == SUDEKIMP_BLACKSMITH_UI_DRAW_TEXT) {
                draw_text(
                    pixels,
                    pitch,
                    viewport_left + command->left,
                    command->top,
                    command->text,
                    command->color,
                    (int)command->text_scale);
            } else {
                return FALSE;
            }
        }
    }
    return TRUE;
}

static BOOL update_blacksmith_ui_texture(
    void *texture,
    const SudekiMpBlacksmithUiSnapshot *snapshot
) {
    void **vtable;
    D3DTextureLockRectFunction lock_rectangle;
    D3DTextureUnlockRectFunction unlock_rectangle;
    SudekiMpBlacksmithUiPresentation presentation;
    SudekiMpD3DLockedRect locked;
    HRESULT result;

    if (texture == NULL || snapshot == NULL) {
        return FALSE;
    }
    vtable = *(void ***)texture;
    if (vtable == NULL ||
        !SudekiMpBlacksmithUiBuildPresentation(snapshot, &presentation)) {
        return FALSE;
    }
    lock_rectangle = (D3DTextureLockRectFunction)
        vtable[D3D_TEXTURE_LOCK_RECT_INDEX];
    unlock_rectangle = (D3DTextureUnlockRectFunction)
        vtable[D3D_TEXTURE_UNLOCK_RECT_INDEX];
    if (lock_rectangle == NULL || unlock_rectangle == NULL ||
        FAILED(lock_rectangle(texture, 0u, &locked, NULL, 0u)) ||
        locked.bits == NULL ||
        locked.pitch < (int)(MENU_TEXTURE_WIDTH * sizeof(uint32_t))) {
        return FALSE;
    }
    if (!draw_blacksmith_ui_presentation(
            (uint32_t *)locked.bits, locked.pitch, &presentation)) {
        (void)unlock_rectangle(texture, 0u);
        return FALSE;
    }
    result = unlock_rectangle(texture, 0u);
    if (FAILED(result)) {
        return FALSE;
    }
    blacksmith_ui_texture_dirty = FALSE;
    blacksmith_ui_last_snapshot = *snapshot;
    return TRUE;
}

static BOOL ensure_blacksmith_ui_texture(
    void *device,
    const SudekiMpBlacksmithUiSnapshot *snapshot
) {
    void **vtable;
    D3DCreateTextureFunction create_texture;
    HRESULT result;

    if (blacksmith_ui_texture_device != device) {
        release_com_object(&blacksmith_ui_texture);
        blacksmith_ui_texture_device = device;
        blacksmith_ui_texture_dirty = TRUE;
    }
    if (memcmp(&blacksmith_ui_last_snapshot, snapshot,
            sizeof(*snapshot)) != 0) {
        blacksmith_ui_texture_dirty = TRUE;
    }
    if (blacksmith_ui_texture != NULL) {
        return !blacksmith_ui_texture_dirty ||
            update_blacksmith_ui_texture(blacksmith_ui_texture, snapshot);
    }
    vtable = *(void ***)device;
    create_texture = vtable == NULL ? NULL : (D3DCreateTextureFunction)
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
        &blacksmith_ui_texture,
        NULL);
    if (FAILED(result) || blacksmith_ui_texture == NULL) {
        blacksmith_ui_texture = NULL;
        return FALSE;
    }
    blacksmith_ui_texture_dirty = TRUE;
    return update_blacksmith_ui_texture(blacksmith_ui_texture, snapshot);
}

static unsigned int cleanroom_vote_participant_count(uint8_t mask) {
    unsigned int count = 0u;

    mask &= SUDEKIMP_TRANSITION_VOTE_PLAYER_MASK;
    while (mask != 0u) {
        count += mask & 1u;
        mask >>= 1u;
    }
    return count;
}

static void cleanroom_vote_seat_status(
    const SudekiMpCleanroomVoteSnapshot *snapshot,
    unsigned int player_index,
    char output[16]
) {
    uint8_t bit = (uint8_t)(1u << player_index);
    const char *state;

    output[0] = '\0';
    if (snapshot == NULL || player_index >= 4u ||
        (snapshot->participant_mask & bit) == 0u) {
        return;
    }
    if ((snapshot->cancelled_mask & bit) != 0u) {
        state = "VETO";
    } else if ((snapshot->accepted_mask & bit) != 0u) {
        state = "READY";
    } else {
        state = "WAIT";
    }
    wsprintfA(output, "P%u %s", player_index + 1u, state);
}

static void cleanroom_vote_status_pair(
    const SudekiMpCleanroomVoteSnapshot *snapshot,
    unsigned int first_player_index,
    char output[40]
) {
    char first[16];
    char second[16];

    cleanroom_vote_seat_status(snapshot, first_player_index, first);
    cleanroom_vote_seat_status(snapshot, first_player_index + 1u, second);
    if (first[0] != '\0' && second[0] != '\0') {
        wsprintfA(output, "%s   %s", first, second);
    } else if (first[0] != '\0') {
        lstrcpyA(output, first);
    } else if (second[0] != '\0') {
        lstrcpyA(output, second);
    } else {
        output[0] = '\0';
    }
}

static BOOL update_transition_vote_texture(
    void *texture,
    const SudekiMpCleanroomVoteSnapshot *snapshot
) {
    void **vtable = *(void ***)texture;
    D3DTextureLockRectFunction lock_rectangle;
    D3DTextureUnlockRectFunction unlock_rectangle;
    SudekiMpD3DLockedRect locked;
    uint32_t *pixels;
    char destination[40];
    char countdown[40];
    char electorate[40];
    char status_primary[40];
    char status_secondary[40];
    const char *title;
    const char *subject_label;
    const char *player_two_prompt;
    const char *player_one_prompt;
    unsigned long remaining_tenths;
    unsigned long seconds;
    unsigned long tenths;
    HRESULT result;

    if (vtable == NULL || snapshot == NULL) {
        return FALSE;
    }
    lock_rectangle = (D3DTextureLockRectFunction)
        vtable[D3D_TEXTURE_LOCK_RECT_INDEX];
    unlock_rectangle = (D3DTextureUnlockRectFunction)
        vtable[D3D_TEXTURE_UNLOCK_RECT_INDEX];
    if (lock_rectangle == NULL || unlock_rectangle == NULL ||
        FAILED(lock_rectangle(texture, 0u, &locked, NULL, 0u)) ||
        locked.bits == NULL ||
        locked.pitch < (int)(TRANSITION_VOTE_OVERLAY_WIDTH *
            sizeof(uint32_t))) {
        return FALSE;
    }
    pixels = (uint32_t *)locked.bits;
    fill_rectangle(
        pixels, locked.pitch, 0, 0,
        TRANSITION_VOTE_OVERLAY_WIDTH,
        TRANSITION_VOTE_OVERLAY_HEIGHT,
        UINT32_C(0xe818202c));
    fill_rectangle(pixels, locked.pitch, 0, 0,
        TRANSITION_VOTE_OVERLAY_WIDTH, 4,
        UINT32_C(0xff35e6e0));
    fill_rectangle(pixels, locked.pitch, 0,
        TRANSITION_VOTE_OVERLAY_HEIGHT - 4,
        TRANSITION_VOTE_OVERLAY_WIDTH,
        TRANSITION_VOTE_OVERLAY_HEIGHT,
        UINT32_C(0xff35e6e0));
    fill_rectangle(pixels, locked.pitch, 0, 0, 4,
        TRANSITION_VOTE_OVERLAY_HEIGHT,
        UINT32_C(0xff35e6e0));
    fill_rectangle(pixels, locked.pitch,
        TRANSITION_VOTE_OVERLAY_WIDTH - 4, 0,
        TRANSITION_VOTE_OVERLAY_WIDTH,
        TRANSITION_VOTE_OVERLAY_HEIGHT,
        UINT32_C(0xff35e6e0));

    memcpy(destination, snapshot->subject,
        sizeof(destination) - 1u);
    destination[sizeof(destination) - 1u] = '\0';
    remaining_tenths =
        (unsigned long)((snapshot->remaining_ms + 99u) / 100u);
    seconds = remaining_tenths / 10u;
    tenths = remaining_tenths % 10u;
    wsprintfA(countdown,
        snapshot->kind == SUDEKIMP_CLEANROOM_VOTE_SAVE_BOOK ?
            "AUTO OPEN IN %lu.%lu" : "AUTO ENTER IN %lu.%lu",
        seconds, tenths);
    wsprintfA(electorate, "PLAYERS %u   MASK %02X",
        cleanroom_vote_participant_count(snapshot->participant_mask),
        (unsigned int)snapshot->participant_mask);
    cleanroom_vote_status_pair(snapshot, 0u, status_primary);
    cleanroom_vote_status_pair(snapshot, 2u, status_secondary);
    title = snapshot->kind == SUDEKIMP_CLEANROOM_VOTE_SAVE_BOOK ?
        "CO-OP SAVE VOTE" : "CO-OP TRAVEL VOTE";
    subject_label = snapshot->kind == SUDEKIMP_CLEANROOM_VOTE_SAVE_BOOK ?
        "REQUEST" : "DESTINATION";
    player_two_prompt =
        snapshot->kind == SUDEKIMP_CLEANROOM_VOTE_SAVE_BOOK ?
            "P2 A ACCEPT   P2 B VETO" :
            "P2 A ACCEPT   P2 B CANCEL";
    player_one_prompt =
        snapshot->kind == SUDEKIMP_CLEANROOM_VOTE_SAVE_BOOK ?
            "P1 ESC VETO" : "P1 ESC CANCEL";

    draw_text(pixels, locked.pitch, 58, 18,
        title, UINT32_C(0xff5ef7f0), 3);
    draw_text(pixels, locked.pitch, 22, 52,
        subject_label, UINT32_C(0xffaab8c8), 2);
    draw_text(pixels, locked.pitch, 22, 72,
        destination[0] == '\0' ? "UNKNOWN" : destination,
        UINT32_C(0xffffffff), 2);
    draw_text(pixels, locked.pitch, 22, 94,
        electorate, UINT32_C(0xffaab8c8), 1);
    draw_text(pixels, locked.pitch, 22, 108,
        status_primary, UINT32_C(0xff7cf29a), 1);
    if (status_secondary[0] != '\0') {
        draw_text(pixels, locked.pitch, 22, 121,
            status_secondary, UINT32_C(0xff7cf29a), 1);
    }
    draw_text(pixels, locked.pitch, 22, 136,
        countdown, UINT32_C(0xffffd166), 2);
    draw_text(pixels, locked.pitch, 22, 159,
        player_two_prompt, UINT32_C(0xffffffff), 1);
    draw_text(pixels, locked.pitch, 22, 178,
        player_one_prompt, UINT32_C(0xffff9b9b), 1);

    result = unlock_rectangle(texture, 0u);
    if (FAILED(result)) {
        return FALSE;
    }
    transition_vote_texture_dirty = FALSE;
    return TRUE;
}

static BOOL ensure_transition_vote_texture(
    void *device,
    const SudekiMpCleanroomVoteSnapshot *snapshot
) {
    void **vtable;
    D3DCreateTextureFunction create_texture;
    uint32_t tenth_seconds;
    HRESULT result;

    if (device == NULL || snapshot == NULL || !snapshot->active) {
        return FALSE;
    }
    tenth_seconds = (snapshot->remaining_ms + 99u) / 100u;
    if (transition_vote_overlay_serial != snapshot->serial ||
        transition_vote_overlay_tenth_seconds != tenth_seconds ||
        transition_vote_overlay_state != snapshot->state ||
        transition_vote_overlay_kind != (unsigned int)snapshot->kind ||
        transition_vote_overlay_participant_mask !=
            snapshot->participant_mask ||
        transition_vote_overlay_accepted_mask != snapshot->accepted_mask ||
        memcmp(transition_vote_overlay_destination,
            snapshot->subject,
            sizeof(transition_vote_overlay_destination)) != 0) {
        transition_vote_overlay_serial = snapshot->serial;
        transition_vote_overlay_tenth_seconds = tenth_seconds;
        transition_vote_overlay_state = snapshot->state;
        transition_vote_overlay_kind = (unsigned int)snapshot->kind;
        transition_vote_overlay_participant_mask =
            snapshot->participant_mask;
        transition_vote_overlay_accepted_mask = snapshot->accepted_mask;
        memcpy(transition_vote_overlay_destination,
            snapshot->subject,
            sizeof(transition_vote_overlay_destination));
        transition_vote_texture_dirty = TRUE;
    }
    if (transition_vote_texture_device != device) {
        release_com_object(&transition_vote_texture);
        transition_vote_texture_device = device;
        transition_vote_texture_dirty = TRUE;
    }
    if (transition_vote_texture != NULL) {
        return !transition_vote_texture_dirty ||
            update_transition_vote_texture(
                transition_vote_texture, snapshot);
    }
    vtable = *(void ***)device;
    create_texture = vtable == NULL ? NULL :
        (D3DCreateTextureFunction)
            vtable[D3D_DEVICE_CREATE_TEXTURE_INDEX];
    if (create_texture == NULL) {
        return FALSE;
    }
    result = create_texture(
        device,
        TRANSITION_VOTE_OVERLAY_WIDTH,
        TRANSITION_VOTE_OVERLAY_HEIGHT,
        1u,
        D3D_USAGE_DYNAMIC,
        D3D_FORMAT_A8R8G8B8,
        D3D_POOL_DEFAULT,
        &transition_vote_texture,
        NULL);
    if (FAILED(result) || transition_vote_texture == NULL) {
        transition_vote_texture = NULL;
        return FALSE;
    }
    transition_vote_texture_dirty = TRUE;
    return update_transition_vote_texture(
        transition_vote_texture, snapshot);
}

static BOOL update_roaming_boundary_texture(
    void *texture,
    const SudekiMpRoamingBoundaryEvaluation *snapshot
) {
    static const int panel_left[2] = {16, 336};
    void **vtable = *(void ***)texture;
    D3DTextureLockRectFunction lock_rectangle;
    D3DTextureUnlockRectFunction unlock_rectangle;
    SudekiMpD3DLockedRect locked;
    uint32_t *pixels;
    uint32_t accent;
    const char *message;
    unsigned int player;
    int fill_width;
    HRESULT result;

    if (vtable == NULL || snapshot == NULL ||
        snapshot->phase < SUDEKIMP_ROAMING_BOUNDARY_WARNING) {
        return FALSE;
    }
    lock_rectangle = (D3DTextureLockRectFunction)
        vtable[D3D_TEXTURE_LOCK_RECT_INDEX];
    unlock_rectangle = (D3DTextureUnlockRectFunction)
        vtable[D3D_TEXTURE_UNLOCK_RECT_INDEX];
    if (lock_rectangle == NULL || unlock_rectangle == NULL ||
        FAILED(lock_rectangle(texture, 0u, &locked, NULL, 0u)) ||
        locked.bits == NULL ||
        locked.pitch < (int)(ROAMING_BOUNDARY_OVERLAY_WIDTH *
            sizeof(uint32_t))) {
        return FALSE;
    }
    pixels = (uint32_t *)locked.bits;
    fill_rectangle(
        pixels, locked.pitch, 0, 0,
        ROAMING_BOUNDARY_OVERLAY_WIDTH,
        ROAMING_BOUNDARY_OVERLAY_HEIGHT,
        UINT32_C(0x00000000));
    accent = snapshot->phase == SUDEKIMP_ROAMING_BOUNDARY_LIMIT ?
        UINT32_C(0xffff6868) : UINT32_C(0xffffd166);
    message = snapshot->phase == SUDEKIMP_ROAMING_BOUNDARY_LIMIT ?
        "MOVE TOWARD PARTY" : "PARTY RANGE WARNING";
    fill_width = (int)(248.0f * snapshot->progress);
    if (fill_width < 0) fill_width = 0;
    if (fill_width > 248) fill_width = 248;
    for (player = 0u; player < 2u; ++player) {
        const int left = panel_left[player];
        const int right = left + 288;
        const char *label = player == 0u ? "P1 PARTY BOUNDARY" :
            "P2 PARTY BOUNDARY";

        fill_rectangle(pixels, locked.pitch,
            left, 402, right, 466, UINT32_C(0xd818202c));
        fill_rectangle(pixels, locked.pitch,
            left, 402, right, 405, accent);
        fill_rectangle(pixels, locked.pitch,
            left, 463, right, 466, accent);
        fill_rectangle(pixels, locked.pitch,
            left, 402, left + 3, 466, accent);
        fill_rectangle(pixels, locked.pitch,
            right - 3, 402, right, 466, accent);
        draw_text(pixels, locked.pitch,
            left + 14, 411, label, UINT32_C(0xffffffff), 1);
        draw_text(pixels, locked.pitch,
            left + 150, 411, message, accent, 1);
        fill_rectangle(pixels, locked.pitch,
            left + 20, 440, right - 20, 454,
            UINT32_C(0xff303b4c));
        fill_rectangle(pixels, locked.pitch,
            left + 20, 440, left + 20 + fill_width, 454, accent);
    }
    result = unlock_rectangle(texture, 0u);
    if (FAILED(result)) {
        return FALSE;
    }
    roaming_boundary_texture_dirty = FALSE;
    return TRUE;
}

static BOOL ensure_roaming_boundary_texture(
    void *device,
    const SudekiMpRoamingBoundaryEvaluation *snapshot
) {
    void **vtable;
    D3DCreateTextureFunction create_texture;
    unsigned int percent;
    HRESULT result;

    if (device == NULL || snapshot == NULL ||
        snapshot->phase < SUDEKIMP_ROAMING_BOUNDARY_WARNING) {
        return FALSE;
    }
    percent = (unsigned int)(snapshot->progress * 100.0f + 0.5f);
    if (roaming_boundary_overlay_phase != snapshot->phase ||
        roaming_boundary_overlay_percent != percent) {
        roaming_boundary_overlay_phase = snapshot->phase;
        roaming_boundary_overlay_percent = percent;
        roaming_boundary_texture_dirty = TRUE;
    }
    if (roaming_boundary_texture_device != device) {
        release_com_object(&roaming_boundary_texture);
        roaming_boundary_texture_device = device;
        roaming_boundary_texture_dirty = TRUE;
    }
    if (roaming_boundary_texture != NULL) {
        return !roaming_boundary_texture_dirty ||
            update_roaming_boundary_texture(
                roaming_boundary_texture, snapshot);
    }
    vtable = *(void ***)device;
    create_texture = vtable == NULL ? NULL :
        (D3DCreateTextureFunction)
            vtable[D3D_DEVICE_CREATE_TEXTURE_INDEX];
    if (create_texture == NULL) {
        return FALSE;
    }
    result = create_texture(
        device,
        ROAMING_BOUNDARY_OVERLAY_WIDTH,
        ROAMING_BOUNDARY_OVERLAY_HEIGHT,
        1u,
        D3D_USAGE_DYNAMIC,
        D3D_FORMAT_A8R8G8B8,
        D3D_POOL_DEFAULT,
        &roaming_boundary_texture,
        NULL);
    if (FAILED(result) || roaming_boundary_texture == NULL) {
        roaming_boundary_texture = NULL;
        return FALSE;
    }
    roaming_boundary_texture_dirty = TRUE;
    return update_roaming_boundary_texture(
        roaming_boundary_texture, snapshot);
}

static BOOL draw_texture_overlay(
    void *texture,
    UINT texture_width,
    UINT texture_height,
    BOOL player_two_badge,
    BOOL roster_card_rectangle,
    BOOL full_screen,
    float roster_left,
    float roster_top,
    float roster_right,
    float roster_bottom,
    int split_vote_viewport
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
    if (split_vote_viewport > 1 ||
        (!roster_card_rectangle && !full_screen &&
         split_vote_viewport < 0 &&
        (description.width < texture_width ||
         description.height < texture_height))) {
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

    if (full_screen) {
        left = -0.5f;
        top = -0.5f;
        right = (float)description.width - 0.5f;
        bottom = (float)description.height - 0.5f;
    } else if (roster_card_rectangle) {
        left = ((float)description.width - (float)MENU_TEXTURE_WIDTH) *
            0.5f + roster_left - 0.5f;
        top = ((float)description.height - (float)MENU_TEXTURE_HEIGHT) *
            0.5f + roster_top - 0.5f;
        right = ((float)description.width - (float)MENU_TEXTURE_WIDTH) *
            0.5f + roster_right - 0.5f;
        bottom = ((float)description.height - (float)MENU_TEXTURE_HEIGHT) *
            0.5f + roster_bottom - 0.5f;
    } else if (split_vote_viewport >= 0) {
        const float viewport_width = (float)description.width * 0.5f;
        const float available_width = viewport_width > 16.0f ?
            viewport_width - 16.0f : viewport_width;
        const float available_height = description.height > 16u ?
            (float)description.height - 16.0f :
            (float)description.height;
        float scale = available_width / (float)texture_width;
        const float height_scale =
            available_height / (float)texture_height;
        float drawn_width;
        float drawn_height;

        if (height_scale < scale) {
            scale = height_scale;
        }
        if (scale > 1.0f) {
            scale = 1.0f;
        }
        if (scale <= 0.0f) {
            release_com_object(&state_block);
            return FALSE;
        }
        drawn_width = (float)texture_width * scale;
        drawn_height = (float)texture_height * scale;
        left = (float)split_vote_viewport * viewport_width +
            (viewport_width - drawn_width) * 0.5f - 0.5f;
        top = ((float)description.height - drawn_height) * 0.5f - 0.5f;
        right = left + drawn_width;
        bottom = top + drawn_height;
    } else if (player_two_badge) {
        left = (float)description.width * 0.75f -
            (float)texture_width * 0.5f - 0.5f;
        top = 14.0f - 0.5f;
    } else {
        left = ((float)description.width - texture_width) * 0.5f - 0.5f;
        top = ((float)description.height - texture_height) * 0.5f - 0.5f;
    }
    /* Card portraits supply their destination rectangle directly.  Their
     * native texture wrapper intentionally has no mod-side width/height
     * metadata, so deriving right/bottom from texture_width here would
     * collapse the quad to 0x0 and make a successfully-resolved portrait
     * invisible. */
    if (!roster_card_rectangle && !full_screen &&
        split_vote_viewport < 0) {
        right = left + texture_width;
        bottom = top + texture_height;
    }
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
        FALSE,
        FALSE,
        FALSE,
        0.0f, 0.0f, 0.0f, 0.0f, -1
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
        TRUE,
        FALSE,
        FALSE,
        0.0f, 0.0f, 0.0f, 0.0f, -1
    );
}

static BOOL draw_blacksmith_ui_overlay(
    const SudekiMpBlacksmithUiSnapshot *snapshot
) {
    void *device;

    if (snapshot == NULL || !snapshot->active ||
        d3d_device_global == NULL ||
        (device = *d3d_device_global) == NULL ||
        !ensure_blacksmith_ui_texture(device, snapshot)) {
        return FALSE;
    }
    return draw_texture_overlay(
        blacksmith_ui_texture,
        MENU_TEXTURE_WIDTH,
        MENU_TEXTURE_HEIGHT,
        FALSE,
        FALSE,
        TRUE,
        0.0f, 0.0f, 0.0f, 0.0f, -1);
}

static void service_player_two_interaction_state(DWORD now) {
    SudekiMpPlayerStatehood *statehood;

    statehood = SudekiMpPlayerStatehoodRuntime();
    SudekiMpPlayerStatehoodService(statehood, (uint32_t)now);
}

static BOOL draw_transition_vote_overlay(
    const SudekiMpCleanroomVoteSnapshot *snapshot
) {
    void *device;
    BOOL player_one_visible;

    if (snapshot == NULL || !snapshot->active ||
        d3d_device_global == NULL ||
        (device = *d3d_device_global) == NULL ||
        !ensure_transition_vote_texture(device, snapshot)) {
        return FALSE;
    }
    if (snapshot->kind != SUDEKIMP_CLEANROOM_VOTE_SAVE_BOOK) {
        return draw_texture_overlay(
            transition_vote_texture,
            TRANSITION_VOTE_OVERLAY_WIDTH,
            TRANSITION_VOTE_OVERLAY_HEIGHT,
            FALSE,
            FALSE,
            FALSE,
            0.0f, 0.0f, 0.0f, 0.0f, -1);
    }
    player_one_visible = draw_texture_overlay(
        transition_vote_texture,
        TRANSITION_VOTE_OVERLAY_WIDTH,
        TRANSITION_VOTE_OVERLAY_HEIGHT,
        FALSE,
        FALSE,
        FALSE,
        0.0f, 0.0f, 0.0f, 0.0f, 0);
    return draw_texture_overlay(
        transition_vote_texture,
        TRANSITION_VOTE_OVERLAY_WIDTH,
        TRANSITION_VOTE_OVERLAY_HEIGHT,
        FALSE,
        FALSE,
        FALSE,
        0.0f, 0.0f, 0.0f, 0.0f, 1) && player_one_visible;
}

static BOOL draw_roaming_boundary_overlay(
    const SudekiMpRoamingBoundaryEvaluation *snapshot
) {
    void *device;

    if (snapshot == NULL ||
        snapshot->phase < SUDEKIMP_ROAMING_BOUNDARY_WARNING ||
        d3d_device_global == NULL ||
        (device = *d3d_device_global) == NULL ||
        !ensure_roaming_boundary_texture(device, snapshot)) {
        return FALSE;
    }
    return draw_texture_overlay(
        roaming_boundary_texture,
        ROAMING_BOUNDARY_OVERLAY_WIDTH,
        ROAMING_BOUNDARY_OVERLAY_HEIGHT,
        FALSE,
        FALSE,
        TRUE,
        0.0f, 0.0f, 0.0f, 0.0f, -1);
}

static BOOL draw_roster_buttons(void) {
    void *device;

    if (!roster_native_screen || d3d_device_global == NULL ||
        (device = *d3d_device_global) == NULL ||
        !ensure_roster_button_texture(device)) {
        return FALSE;
    }
    return draw_texture_overlay(
        roster_button_texture,
        MENU_TEXTURE_WIDTH,
        MENU_TEXTURE_HEIGHT,
        FALSE,
        FALSE,
        FALSE,
        0.0f, 0.0f, 0.0f, 0.0f, -1);
}

static BOOL draw_roster_backdrop(void) {
    void *device;

    if (!roster_native_screen || d3d_device_global == NULL ||
        (device = *d3d_device_global) == NULL ||
        !ensure_roster_backdrop_texture(device)) {
        return FALSE;
    }
    return draw_texture_overlay(
        roster_backdrop_texture,
        ROSTER_BACKDROP_WIDTH,
        ROSTER_BACKDROP_HEIGHT,
        FALSE,
        FALSE,
        TRUE,
        0.0f, 0.0f, 0.0f, 0.0f, -1);
}

static void draw_roster_native_portraits(void) {
    unsigned int actor;

    if (roster_native_screen_kind != NATIVE_ROSTER_PLAYER_ONE &&
        roster_native_screen_kind != NATIVE_ROSTER_PLAYER_TWO) {
        return;
    }
    for (actor = 0u; actor < MENU_ACTOR_COUNT; ++actor) {
        const float card_left = 40.0f + (float)actor * 144.0f;
        void *texture = roster_native_portrait_gpu_textures[actor];
        const BOOL unavailable = roster_native_screen_kind ==
            NATIVE_ROSTER_PLAYER_TWO &&
            (unsigned int)roster_display_actor(actor) == roster_player_one;

        if (texture == NULL || unavailable) {
            continue;
        }
        /* The card interior is 120x77 after its native-style border.  Keep
         * the square head centered and leave the colored selection ring
         * visible around it. */
        (void)draw_texture_overlay(
            texture,
            64u,
            64u,
            FALSE,
            TRUE,
            FALSE,
            card_left + 25.0f,
            310.0f,
            card_left + 101.0f,
            386.0f,
            -1);
    }
}

static void __attribute__((stdcall)) cleanroom_ui_scene_render(
    void *scene
) {
    unsigned int actor;

    /* The Load Game page must remain alive while it owns the decoded portrait
     * materials.  Native updates can re-enable its CycleIcon anchors, so hide
     * those four source widgets again at the last presentation boundary. */
    if (roster_native_screen) {
        for (actor = 0u; actor < MENU_ACTOR_COUNT; ++actor) {
            roster_hide_native_portrait_anchor(
                roster_native_portrait_icons[actor]);
        }
    }
    /* Drain the ordinary title/Load Game queue first.  Destroying that page
     * invalidates its portrait owners and previously crashed on the following
     * frame; covering its completed draw is presentation-only and reversible. */
    if (original_ui_scene_render != NULL) {
        original_ui_scene_render(scene);
    }
    if (roster_native_screen) {
        /* Replace both native backing pages with one clean roster canvas, then
         * compose only the independent cards and borrowed head textures. */
        (void)draw_roster_backdrop();
        (void)draw_roster_buttons();
        draw_roster_native_portraits();

        /* The title font is still Sudeki's native queued-font renderer.  The
         * earlier queue was intentionally covered along with the backing
         * pages, so submit our labels once more and flush only that fresh
         * queue on top of the completed roster composition. */
        native_roster_submit_page();
        if (original_ui_scene_render != NULL) {
            original_ui_scene_render(scene);
        }
    }
}

static void trace_pc_front_end_global_once(void) {
    static BOOL raw_initialized;
    static void *last_raw_controller;
    static void *last_raw_vtable;
    static void *last_controller;
    static unsigned int last_stage = ~0u;
    static unsigned int last_mode = ~0u;
    static unsigned int last_selection = ~0u;
    static unsigned int last_flags = ~0u;
    void *controller;
    unsigned int stage;
    unsigned int mode;
    unsigned int selection;
    unsigned int flags;

    if (!roster_mode || game_base == NULL) {
        return;
    }
    /* The native title controller is created after the DLL/monitor starts.
     * Force a fresh read on every sample: this storage is mutated by Sudeki,
     * outside the C abstract machine, so an ordinary optimized load may be
     * hoisted out of the polling loop and preserve the startup NULL forever. */
    controller = *(void * volatile *)(game_base + RVA_PC_FRONT_END_GLOBAL);
    if (controller == NULL) {
        if (roster_resume_committed) {
            roster_resume_committed = FALSE;
            roster_resume_controller = NULL;
            SudekiMpLogWrite(
                "cleanroom_menu event=native_roster state=resume_guard_cleared "
                "reason=pc_front_end_released\r\n");
        }
        if (!raw_initialized || last_raw_controller != NULL) {
            SudekiMpLogWrite(
                "cleanroom_menu event=pc_front_end_global status=absent\r\n");
            raw_initialized = TRUE;
            last_raw_controller = NULL;
            last_raw_vtable = NULL;
        }
        return;
    }
    if (!raw_initialized || controller != last_raw_controller ||
        *(void **)controller != last_raw_vtable) {
        uintptr_t actual_vtable = (uintptr_t)*(void **)controller;
        uintptr_t base_value = (uintptr_t)game_base;
        SudekiMpLogFormat(
            "cleanroom_menu event=pc_front_end_global status=observed "
            "controller=%p actual_vtable=%p actual_vtable_rva=0x%08lx "
            "expected_vtable_rva=0x%08lx\r\n",
            controller,
            *(void **)controller,
            actual_vtable >= base_value ?
                (unsigned long)(actual_vtable - base_value) : ~0ul,
            (unsigned long)RVA_PC_FRONT_END_VTABLE);
        raw_initialized = TRUE;
        last_raw_controller = controller;
        last_raw_vtable = *(void **)controller;
    }
    if (*(void **)controller !=
        (void *)(game_base + RVA_PC_FRONT_END_VTABLE)) {
        return;
    }
    stage = *(unsigned int *)((uint8_t *)controller + 0x2cu);
    mode = *(unsigned int *)((uint8_t *)controller + 0x4cu);
    selection = *(unsigned int *)((uint8_t *)controller + 0x6a4u);
    flags = *(unsigned char *)((uint8_t *)controller + 0x6b8u);
    if (controller == last_controller && stage == last_stage &&
        mode == last_mode && selection == last_selection &&
        flags == last_flags) {
        return;
    }
    SudekiMpLogFormat(
        "cleanroom_menu event=pc_front_end_global controller=%p "
        "vtable_rva=0x%08lx stage=%lu mode=%lu selection=%lu "
        "flags=0x%02lx\r\n",
        controller,
        (unsigned long)RVA_PC_FRONT_END_VTABLE,
        (unsigned long)stage,
        (unsigned long)mode,
        (unsigned long)selection,
        (unsigned long)flags);
    last_controller = controller;
    last_stage = stage;
    last_mode = mode;
    last_selection = selection;
    last_flags = flags;
}

static DWORD WINAPI pc_front_end_trace_thread_main(void *unused) {
    (void)unused;
    while (InterlockedCompareExchange(&pc_front_end_trace_stop, 0, 0) == 0) {
        trace_pc_front_end_global_once();
        Sleep(50u);
    }
    return 0u;
}

static void cleanroom_vote_snapshot_from_zone(
    SudekiMpCleanroomVoteSnapshot *output,
    const SudekiMpZoneTransitionVoteSnapshot *source
) {
    ZeroMemory(output, sizeof(*output));
    output->active = source->active;
    output->kind = SUDEKIMP_CLEANROOM_VOTE_TRAVEL;
    output->state = source->state;
    output->serial = source->serial;
    output->remaining_ms = source->remaining_ms;
    output->participant_mask = source->participant_mask;
    output->accepted_mask = source->accepted_mask;
    output->cancelled_mask = source->cancelled_mask;
    memcpy(output->subject, source->destination,
        sizeof(output->subject));
}

static void cleanroom_vote_snapshot_from_save_book(
    SudekiMpCleanroomVoteSnapshot *output,
    const SudekiMpSaveBookVoteSnapshot *source
) {
    ZeroMemory(output, sizeof(*output));
    output->active = source->active;
    output->kind = SUDEKIMP_CLEANROOM_VOTE_SAVE_BOOK;
    output->state = source->state;
    output->serial = source->serial;
    output->remaining_ms =
        source->state == SUDEKIMP_SAVE_BOOK_VOTE_AWAITING_VISIBLE ?
            SUDEKIMP_SAVE_BOOK_VOTE_TIMEOUT_MS : source->remaining_ms;
    output->participant_mask = source->participant_mask;
    output->accepted_mask = source->accepted_mask;
    output->cancelled_mask = source->cancelled_mask;
    lstrcpyA(output->subject, "OPEN SAVE MENU");
}

void SudekiMpCleanroomFrameEndDispatch(void) {
    /* Keep focus/loading suspension responsive even when the gameplay
     * controller is not ticking during a front-end or transition frame. */
    service_story_test_boost();
    SudekiMpCleanroomMenuRender();
    original_frame_end();
}

void SudekiMpCleanroomMenuRender(void) {
    SudekiMpZoneTransitionVoteSnapshot vote_snapshot;
    SudekiMpSaveBookVoteSnapshot save_book_snapshot;
    SudekiMpCleanroomVoteSnapshot cleanroom_vote_snapshot;
    SudekiMpRoamingBoundaryEvaluation boundary_snapshot;
    SudekiMpBlacksmithUiSnapshot blacksmith_snapshot;
    BOOL vote_snapshot_valid;
    BOOL save_book_snapshot_valid;
    BOOL save_book_vote_active;
    BOOL boundary_snapshot_valid;
    BOOL shared_interaction_modal_active;
    BOOL blacksmith_ui_active;
    BOOL blacksmith_snapshot_valid;

    if (game_base == NULL) {
        return;
    }
    vote_snapshot_valid = SudekiMpZoneTransitionGetVoteSnapshot(
        &vote_snapshot);
    save_book_snapshot_valid = SudekiMpSaveBookGetVoteSnapshot(
        &save_book_snapshot);
    save_book_vote_active = save_book_snapshot_valid &&
        save_book_snapshot.active;
    boundary_snapshot_valid =
        SudekiMpControlSeparationGetRoamingBoundarySnapshot(
            &boundary_snapshot);
    SudekiMpBlacksmithUiAdapterService();
    blacksmith_ui_active = SudekiMpBlacksmithUiAdapterActive();
    blacksmith_snapshot_valid = blacksmith_ui_active &&
        SudekiMpBlacksmithUiAdapterGetSnapshot(&blacksmith_snapshot);
    service_player_two_interaction_state(GetTickCount());
    shared_interaction_modal_active =
        SudekiMpSplitScreenSharedInteractionModalActive() ||
        blacksmith_ui_active;
    if (integrated_multiplayer_mode) {
        /* In integrated mode the split compositor owns the frame-end hook,
         * so this overlay callback is also the loading/focus-safe service
         * point for restoring the optional story fast-forward lease. */
        service_story_test_boost();
        {
            BOOL requested =
                SudekiMpControlSeparationPlayerTwoRequested();
            BOOL active = SudekiMpControlSeparationPlayerTwoActive();
            BOOL input_ready = SudekiMpControlSeparationInputReady();
            BOOL participation_requested =
                SudekiMpSplitScreenRosterParticipationRequested();

            if (multiplayer_requested != requested ||
                multiplayer_active != active ||
                multiplayer_input_ready != input_ready ||
                multiplayer_participation_requested !=
                    participation_requested) {
                multiplayer_requested = requested;
                multiplayer_active = active;
                multiplayer_input_ready = input_ready;
                multiplayer_participation_requested =
                    participation_requested;
                player_two_badge_dirty = TRUE;
            }
        }
    }
    if (integrated_multiplayer_mode &&
        !save_book_vote_active &&
        !shared_interaction_modal_active &&
        SudekiMpSplitScreenRosterParticipationAvailable() &&
        SudekiMpCleanroomEngineWorldReady() &&
        !draw_player_two_badge() && !player_two_badge_failure_logged) {
        player_two_badge_failure_logged = TRUE;
        SudekiMpLogWrite(
            "cleanroom_menu event=player_two_badge status=unavailable "
            "fallback=split_screen_unchanged\r\n"
        );
    }
    if (integrated_multiplayer_mode && !save_book_vote_active &&
        !shared_interaction_modal_active &&
        boundary_snapshot_valid &&
        boundary_snapshot.phase >= SUDEKIMP_ROAMING_BOUNDARY_WARNING) {
        BOOL overlay_visible =
            draw_roaming_boundary_overlay(&boundary_snapshot);

        SudekiMpControlSeparationReportRoamingBoundaryOverlay(
            overlay_visible);
        if (!overlay_visible &&
            !roaming_boundary_overlay_failure_logged) {
            roaming_boundary_overlay_failure_logged = TRUE;
            SudekiMpLogWrite(
                "cleanroom_menu event=roaming_boundary_overlay status=unavailable fallback=advisory_only_no_hidden_movement_clamp\r\n");
        }
    } else {
        SudekiMpControlSeparationReportRoamingBoundaryOverlay(FALSE);
        roaming_boundary_overlay_failure_logged = FALSE;
    }
    if (!save_book_vote_active && !blacksmith_ui_active && menu_open &&
        !draw_menu_overlay() && !overlay_failure_logged) {
        overlay_failure_logged = TRUE;
        SudekiMpLogWrite(
            "cleanroom_menu event=overlay status=unavailable "
            "fallback=gameplay_unchanged\r\n"
        );
    }
    if (save_book_vote_active && blacksmith_ui_active) {
        (void)SudekiMpSaveBookReportVoteOverlay(
            save_book_snapshot.serial, FALSE);
        save_book_vote_overlay_failure_logged = TRUE;
        SudekiMpLogWrite(
            "save_book_vote event=overlay status=blocked "
            "reason=blacksmith_ui_active "
            "policy=cancel_save_vote_preserve_existing_modal\r\n");
        transition_vote_overlay_failure_logged = FALSE;
    } else if (save_book_vote_active) {
        BOOL overlay_visible;
        BOOL overlay_reported;

        cleanroom_vote_snapshot_from_save_book(
            &cleanroom_vote_snapshot, &save_book_snapshot);
        overlay_visible = owns_foreground() &&
            draw_transition_vote_overlay(&cleanroom_vote_snapshot);
        overlay_reported = SudekiMpSaveBookReportVoteOverlay(
            save_book_snapshot.serial, overlay_visible);
        if (overlay_visible && overlay_reported &&
            save_book_vote_input.player_two.serial ==
                save_book_snapshot.serial) {
            save_book_vote_input.overlay_acknowledged = TRUE;
        }
        if (!overlay_visible &&
            !save_book_vote_overlay_failure_logged) {
            save_book_vote_overlay_failure_logged = TRUE;
            SudekiMpLogWrite(
                "save_book_vote event=overlay status=unavailable "
                "fallback=cancel_request_never_open_native_save_ui\r\n");
        } else if (overlay_visible) {
            save_book_vote_overlay_failure_logged = FALSE;
        }
        if (vote_snapshot_valid && vote_snapshot.active) {
            (void)SudekiMpZoneTransitionReportVoteOverlay(
                vote_snapshot.serial, FALSE);
            SudekiMpLogWrite(
                "save_book_vote event=overlay conflict=travel_vote "
                "resolution=save_book_priority_travel_fail_closed\r\n");
        }
        transition_vote_overlay_failure_logged = FALSE;
    } else if (!blacksmith_ui_active &&
        vote_snapshot_valid && vote_snapshot.active) {
        BOOL overlay_visible;

        cleanroom_vote_snapshot_from_zone(
            &cleanroom_vote_snapshot, &vote_snapshot);
        overlay_visible = draw_transition_vote_overlay(
            &cleanroom_vote_snapshot);

        (void)SudekiMpZoneTransitionReportVoteOverlay(
            vote_snapshot.serial, overlay_visible);
        if (!overlay_visible &&
            !transition_vote_overlay_failure_logged) {
            transition_vote_overlay_failure_logged = TRUE;
            SudekiMpLogWrite(
                "transition_vote event=overlay status=unavailable "
                "fallback=cancel_request_and_remain_outside\r\n");
        }
    } else {
        transition_vote_overlay_failure_logged = FALSE;
        if (!save_book_vote_active) {
            save_book_vote_overlay_failure_logged = FALSE;
        }
    }
    if (blacksmith_ui_active) {
        BOOL overlay_visible = blacksmith_snapshot_valid &&
            draw_blacksmith_ui_overlay(&blacksmith_snapshot);

        SudekiMpBlacksmithUiAdapterReportOverlay(overlay_visible);
        if (!overlay_visible && !blacksmith_ui_overlay_failure_logged) {
            blacksmith_ui_overlay_failure_logged = TRUE;
            SudekiMpLogWrite(
                "blacksmith_ui event=overlay status=unavailable "
                "fallback=close_all_panels_release_script_and_world_input\r\n");
        } else if (overlay_visible) {
            blacksmith_ui_overlay_failure_logged = FALSE;
        }
    } else {
        blacksmith_ui_overlay_failure_logged = FALSE;
    }
}

__attribute__((naked, noinline, used))
static void cleanroom_frame_end_entry(void) {
    __asm__ volatile(
        "call _SudekiMpCleanroomFrameEndDispatch\n\t"
        "ret\n\t"
    );
}

static BOOL __attribute__((cdecl)) cleanroom_movie_play(
    const char *movie_name,
    BOOL skippable
) {
    static const char *const startup_movies[] = {
        "Publisher.bik", "ClimaxLogo.bik", "TWIMTBP.bik"
    };
    unsigned int index;
    static char last_movie_name[128];
    char observed_movie_name[128];

    observed_movie_name[0] = '\0';
    if (movie_name != NULL && menu_memory_readable(movie_name, 1u)) {
        size_t length = 0u;
        while (length + 1u < sizeof(observed_movie_name) &&
            menu_memory_readable(movie_name + length, 1u) &&
            movie_name[length] != '\0') {
            ++length;
        }
        if (length != 0u && length + 1u < sizeof(observed_movie_name)) {
            memcpy(observed_movie_name, movie_name, length);
            observed_movie_name[length] = '\0';
        }
    }
    if (observed_movie_name[0] != '\0' &&
        strcmp(observed_movie_name, last_movie_name) != 0) {
        strncpy(last_movie_name, observed_movie_name,
            sizeof(last_movie_name) - 1u);
        last_movie_name[sizeof(last_movie_name) - 1u] = '\0';
        SudekiMpLogFormat(
            "cleanroom_menu event=movie_play_observed name=%s "
            "skippable=%s roster_screen=%s pid=%lu\r\n",
            observed_movie_name,
            skippable ? "true" : "false",
            roster_native_screen ? "true" : "false",
            (unsigned long)GetCurrentProcessId());
    }

    for (index = 0u;
         movie_name != NULL && index < sizeof(startup_movies) /
             sizeof(startup_movies[0]);
         ++index) {
        size_t length = strlen(startup_movies[index]) + 1u;
        if (menu_memory_readable(movie_name, length) &&
            memcmp(movie_name, startup_movies[index], length) == 0) {
            SudekiMpLogFormat(
                "cleanroom_menu event=startup_movie_skip movie=%s "
                "scope=title_roster_development_only\r\n",
                movie_name);
            return TRUE;
        }
    }
    return original_movie_play == NULL ? FALSE :
        original_movie_play(movie_name, skippable);
}

static BOOL install_cleanroom_menu_internal(
    HMODULE game_module,
    UINT toggle_key,
    BOOL integrated,
    BOOL roster,
    BOOL skip_startup_movies,
    BOOL traversal,
    BOOL enable_story_test_boost,
    UINT configured_story_test_boost_key,
    float configured_story_test_boost_multiplier
) {
    uint8_t *base;
    void **controller_slot;

    if (game_module == NULL || game_base != NULL || toggle_key == 0u ||
        toggle_key > 0xffu ||
        (!roster && !traversal && !command_line_is_cleanroom()) ||
        (enable_story_test_boost &&
         (!roster || configured_story_test_boost_key == 0u ||
          configured_story_test_boost_key > 0xffu ||
          !isfinite(configured_story_test_boost_multiplier) ||
          configured_story_test_boost_multiplier < 1.0f ||
          configured_story_test_boost_multiplier > 4.0f))) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    if (!SudekiMpCleanroomEngineInitialize(game_module) ||
        (!roster && !traversal && !SudekiMpCleanroomAudioInitialize())) {
        SudekiMpCleanroomEngineReset();
        return FALSE;
    }
    if (!roster && !traversal &&
        (!SudekiMpCleanroomEngineSetInfiniteSp(TRUE) ||
        !SudekiMpCleanroomEngineSetInfiniteSpirit(TRUE) ||
        !SudekiMpCleanroomEngineSetInfiniteJetpackFuel(TRUE))) {
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
    story_test_boost_enabled = enable_story_test_boost;
    story_test_boost_active = FALSE;
    story_test_boost_runtime_applied = FALSE;
    story_test_boost_key_was_down = FALSE;
    story_test_boost_failure_logged = FALSE;
    story_test_boost_key = configured_story_test_boost_key;
    story_test_boost_multiplier = configured_story_test_boost_multiplier;
    menu_open = FALSE;
    menu_texture_dirty = TRUE;
    player_two_badge_dirty = TRUE;
    transition_vote_texture = NULL;
    transition_vote_texture_device = NULL;
    transition_vote_texture_dirty = TRUE;
    transition_vote_overlay_failure_logged = FALSE;
    save_book_vote_overlay_failure_logged = FALSE;
    transition_vote_overlay_serial = 0u;
    transition_vote_overlay_tenth_seconds = 0u;
    transition_vote_overlay_state = 0u;
    transition_vote_overlay_kind = 0u;
    transition_vote_overlay_participant_mask = 0u;
    transition_vote_overlay_accepted_mask = 0u;
    ZeroMemory(transition_vote_overlay_destination,
        sizeof(transition_vote_overlay_destination));
    reset_save_book_vote_input();
    roster_button_texture_dirty = TRUE;
    roster_backdrop_texture = NULL;
    roster_backdrop_texture_device = NULL;
    ZeroMemory(roster_native_portrait_textures,
        sizeof(roster_native_portrait_textures));
    ZeroMemory(roster_native_portrait_gpu_textures,
        sizeof(roster_native_portrait_gpu_textures));
    ZeroMemory(roster_native_portrait_icons,
        sizeof(roster_native_portrait_icons));
    roster_native_portrait_owner = NULL;
    roster_native_portrait_controller = NULL;
    roster_native_portraits_requested = FALSE;
    roster_native_portrait_request_failed = FALSE;
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
    infinite_jetpack_fuel = TRUE;
    infinite_jetpack_fuel_valid = TRUE;
    integrated_multiplayer_mode = integrated;
    roster_mode = roster;
    loaded_save_coop_autostart_enabled = FALSE;
    loaded_save_coop_autostart_roster_published = FALSE;
    loaded_save_coop_autostart_last_locked = FALSE;
    zone_traversal_mode = traversal;
    zone_traversal_page = ZONE_TRAVERSAL_PAGE_WORLDS;
    zone_traversal_selection = 0u;
    zone_traversal_waiting = FALSE;
    zone_traversal_waiting_since = 0u;
    zone_traversal_waiting_world[0] = '\0';
    zone_traversal_transition_guard_until = 0u;
    roster_locked = FALSE;
    roster_coop_profile = FALSE;
    roster_talos_tuning_enabled = FALSE;
    roster_talos_health_scale = 2u;
    roster_talos_stagger_limit = 10u;
    roster_talos_stagger_window = 10u;
    roster_player_one = SUDEKIMP_CLEANROOM_AILISH;
    roster_player_two = SUDEKIMP_CLEANROOM_TAL;
    roster_cursor = 0u;
    multiplayer_requested = FALSE;
    multiplayer_active = FALSE;
    multiplayer_input_ready = FALSE;
    multiplayer_participation_requested = FALSE;
    coop_role_lock_active = FALSE;
    coop_selected_actor = SUDEKIMP_CLEANROOM_TAL;
    coop_ready_failed_until = 0u;
    coop_lobby_prompted = FALSE;
    last_status_update = 0u;
    roster_build_persistence_path();
    roster_load_persistence();
    menu_open = FALSE;
    roster_waiting_new_game = FALSE;
    roster_replaying_new_game = FALSE;
    roster_resume_committed = FALSE;
    roster_resume_controller = NULL;
    roster_native_screen = FALSE;
    roster_native_screen_kind = NATIVE_ROSTER_NONE;
    roster_native_selection = 0u;
    roster_native_screen_dirty = FALSE;
    roster_confirm_input_armed = FALSE;
    roster_native_transition_started = 0u;
    roster_native_transition_from_title = FALSE;
    roster_pending_controller = NULL;
    roster_original_menu_valid = FALSE;
    roster_original_menu_count = 0u;
    roster_original_menu_selection = 0u;
    ZeroMemory(roster_native_label_objects,
        sizeof(roster_native_label_objects));
    roster_native_label_states_valid = FALSE;
    roster_native_rows_active = FALSE;
    ZeroMemory(roster_native_page_object,
        sizeof(roster_native_page_object));
    ZeroMemory(roster_native_page_vtable,
        sizeof(roster_native_page_vtable));
    roster_native_page_contract_ready = FALSE;
    roster_native_page_state_active = FALSE;
    roster_native_page_takeover_pending = FALSE;
    roster_native_page_leave_requested = FALSE;
    roster_native_page_leave_started_ms = 0u;
    native_save_page_input_suppression_logged = FALSE;
    roster_native_page_controller = NULL;
    roster_native_page_saved_active = NULL;
    roster_native_page_saved_backing = NULL;
    roster_native_page_saved_state = 0u;
    roster_native_page_saved_previous = 0u;
    roster_native_page_saved_mode = 0u;
    ZeroMemory(roster_native_options_rows,
        sizeof(roster_native_options_rows));
    roster_native_options_rows_bound = FALSE;
    original_front_end_action = NULL;
    original_front_end_update = NULL;
    original_front_end_render = NULL;
    original_ui_scene_render = NULL;
    original_front_end_state_update = roster ?
        (FrontEndStateUpdateFunction)(base + RVA_FRONT_END_STATE_UPDATE) : NULL;
    front_end_menu_builder = roster ?
        (FrontEndMenuBuilderFunction)(base + RVA_FRONT_END_MENU_BUILDER) : NULL;
    front_end_selection_refresh = roster ?
        (FrontEndSelectionRefreshFunction)(base + RVA_FRONT_END_SELECTION_REFRESH) : NULL;
    if (!roster && !traversal) {
        update_action_status();
    }

    if (traversal && skip_startup_movies) {
        static const uint8_t traversal_movie_play_entry[] = {
            0x55, 0x8b, 0xec, 0x83, 0xe4, 0xf8
        };
        if (!SudekiMpInstallInlineHook(
                &startup_movie_hook,
                base + RVA_MOVIE_PLAY,
                traversal_movie_play_entry,
                sizeof(traversal_movie_play_entry),
                (const void *)cleanroom_movie_play)) {
            SudekiMpUninstallCleanroomMenu();
            return FALSE;
        }
        original_movie_play =
            (MoviePlayFunction)startup_movie_hook.trampoline;
        if (menu_memory_executable(base + RVA_MOVIE_STOP)) {
            MovieStopFunction stop_movie =
                (MovieStopFunction)(base + RVA_MOVIE_STOP);
            (void)stop_movie();
        }
        SudekiMpLogFormat(
            "cleanroom_menu event=startup_movie_skip status=installed "
            "rva=0x%08lx policy=traversal_world_test\r\n",
            (unsigned long)RVA_MOVIE_PLAY);
    }

    if (roster) {
        static const uint8_t movie_play_entry[] = {
            0x55, 0x8b, 0xec, 0x83, 0xe4, 0xf8
        };
        static const uint8_t front_end_action_entry[] = {
            0x8b, 0x44, 0x24, 0x04, 0x57
        };
        SudekiMpLogFormat(
            "cleanroom_menu event=native_portrait_selector_trace "
            "status=disabled rva=0x%08lx "
            "policy=direct_exact_native_calls_no_diagnostic_detour\r\n",
            (unsigned long)RVA_HUD_PORTRAIT_RESOURCE_SELECT
        );
        {
            static const uint8_t save_entry_update_entry[] = {
                0x57, 0x8b, 0xf8, 0x8b, 0x47, 0x48
            };
            if (!SudekiMpInstallInlineHook(
                    &native_save_entry_update_hook,
                    base + RVA_NATIVE_SAVE_ENTRY_UPDATE,
                    save_entry_update_entry,
                    sizeof(save_entry_update_entry),
                    (const void *)native_save_entry_update_entry)) {
                SudekiMpUninstallCleanroomMenu();
                return FALSE;
            }
            native_save_entry_update_trampoline =
                native_save_entry_update_hook.trampoline;
            SudekiMpLogFormat(
                "cleanroom_menu event=native_save_entry_trace status=installed "
                "rva=0x%08lx length=%lu policy=observation_only\r\n",
                (unsigned long)RVA_NATIVE_SAVE_ENTRY_UPDATE,
                (unsigned long)sizeof(save_entry_update_entry)
            );
        }
        {
            static const uint8_t save_page_action_entry[] = {
                /* push ebx; mov ebx,ecx;
                 * cmp byte ptr [ebx+0x10c],0
                 *
                 * The compare is seven bytes by itself.  A six-byte hook
                 * split it after the displacement's first byte, producing
                 * an invalid trampoline as soon as the native save page
                 * forwarded an action. */
                0x53, 0x8b, 0xd9,
                0x80, 0xbb, 0x0c, 0x01, 0x00, 0x00, 0x00
            };
            if (!SudekiMpInstallInlineHook(
                    &native_save_page_action_hook,
                    base + RVA_NATIVE_SAVE_PAGE_ACTION,
                    save_page_action_entry,
                    sizeof(save_page_action_entry),
                    (const void *)native_save_page_action_entry)) {
                SudekiMpUninstallCleanroomMenu();
                return FALSE;
            }
            native_save_page_action_trampoline =
                native_save_page_action_hook.trampoline;
            SudekiMpLogFormat(
                "cleanroom_menu event=native_save_page_action_trace "
                "status=installed rva=0x%08lx length=%lu "
                "policy=observation_only\r\n",
                (unsigned long)RVA_NATIVE_SAVE_PAGE_ACTION,
                (unsigned long)sizeof(save_page_action_entry)
            );
        }
        {
            static const uint8_t save_page_input_entry[] = {
                0x55, 0x8b, 0xec, 0x83, 0xe4, 0xf8
            };
            if (!SudekiMpInstallInlineHook(
                    &native_save_page_input_hook,
                    base + RVA_NATIVE_SAVE_PAGE_INPUT,
                    save_page_input_entry,
                    sizeof(save_page_input_entry),
                    (const void *)native_save_page_input_entry)) {
                SudekiMpUninstallCleanroomMenu();
                return FALSE;
            }
            native_save_page_input_trampoline =
                native_save_page_input_hook.trampoline;
            SudekiMpLogFormat(
                "cleanroom_menu event=native_save_page_input_trace "
                "status=installed rva=0x%08lx length=%lu "
                "policy=observation_only\r\n",
                (unsigned long)RVA_NATIVE_SAVE_PAGE_INPUT,
                (unsigned long)sizeof(save_page_input_entry)
            );
        }
        if (skip_startup_movies) {
            if (!SudekiMpInstallInlineHook(
                    &startup_movie_hook,
                    base + RVA_MOVIE_PLAY,
                    movie_play_entry,
                    sizeof(movie_play_entry),
                    (const void *)cleanroom_movie_play)) {
                SudekiMpUninstallCleanroomMenu();
                return FALSE;
            }
            original_movie_play =
                (MoviePlayFunction)startup_movie_hook.trampoline;
            if (menu_memory_executable(base + RVA_MOVIE_STOP)) {
                MovieStopFunction stop_movie =
                    (MovieStopFunction)(base + RVA_MOVIE_STOP);
                BOOL stopped = stop_movie();
                SudekiMpLogFormat(
                    "cleanroom_menu event=startup_movie_skip "
                    "phase=stop_already_active_first_logo result=%s "
                    "stop_rva=0x%08lx\r\n",
                    stopped ? "true" : "false",
                    (unsigned long)RVA_MOVIE_STOP);
            }
            SudekiMpLogFormat(
                "cleanroom_menu event=startup_movie_skip status=installed "
                "rva=0x%08lx policy=three_known_logo_movies_only\r\n",
                (unsigned long)RVA_MOVIE_PLAY);
        }
        original_ui_scene_render = (UiSceneRenderFunction)(
            base + RVA_UI_SCENE_RENDER);
        if (!SudekiMpInstallRelativeCallHook(
                &roster_ui_scene_render_hook,
                base + RVA_UI_SCENE_RENDER_CALL,
                (const void *)original_ui_scene_render,
                (const void *)cleanroom_ui_scene_render)) {
            SudekiMpUninstallCleanroomMenu();
            return FALSE;
        }
        if (!SudekiMpInstallInlineHook(
                &front_end_action_hook,
                base + RVA_FRONT_END_ACTION,
                front_end_action_entry,
                sizeof(front_end_action_entry),
                (const void *)cleanroom_front_end_action)) {
            SudekiMpUninstallCleanroomMenu();
            return FALSE;
        }
        original_front_end_action =
            (FrontEndActionFunction)front_end_action_hook.trampoline;
        static const uint8_t front_end_update_entry[] = {
            0x55, 0x8b, 0xec, 0x83, 0xe4, 0xf8, 0x83, 0xec, 0x0c
        };
        if (!SudekiMpInstallInlineHook(
                &front_end_update_hook,
                base + RVA_FRONT_END_UPDATE,
                front_end_update_entry,
                sizeof(front_end_update_entry),
                (const void *)cleanroom_pc_front_end_update_trace)) {
            SudekiMpUninstallCleanroomMenu();
            return FALSE;
        }
        original_front_end_update =
            (FrontEndUpdateFunction)front_end_update_hook.trampoline;
        original_front_end_render =
            (FrontEndRenderFunction)(base + RVA_FRONT_END_RENDER);
        if (!SudekiMpInstallPointerHook(
                &front_end_render_hook,
                (void **)(base + RVA_FRONT_END_RENDER_VTABLE_SLOT),
                original_front_end_render,
                cleanroom_pc_front_end_render)) {
            SudekiMpUninstallCleanroomMenu();
            return FALSE;
        }
        SudekiMpLogFormat(
            "cleanroom_menu event=native_roster_action_hook status=installed "
            "kind=inline entry_rva=0x%08lx length=%lu original=%p "
            "replacement=%p vtable_slot_rva=0x%08lx "
            "update_slot_rva=0x%08lx update_rva=0x%08lx "
            "render_slot_rva=0x%08lx render_rva=0x%08lx\\r\\n",
            (unsigned long)RVA_FRONT_END_ACTION,
            (unsigned long)sizeof(front_end_action_entry),
            (void *)original_front_end_action,
            (void *)cleanroom_front_end_action,
            (unsigned long)RVA_FRONT_END_ACTION_VTABLE_SLOT,
            (unsigned long)RVA_FRONT_END_UPDATE_VTABLE_SLOT,
            (unsigned long)RVA_FRONT_END_UPDATE,
            (unsigned long)RVA_FRONT_END_RENDER_VTABLE_SLOT,
            (unsigned long)RVA_FRONT_END_RENDER);
        /* The state-update entry is not hooked.  Its exact address is retained
         * only for the EAX bridge that reuses native state 10's visual fade
         * without changing the title controller's logical state. */
    }

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
    if (roster) {
        InterlockedExchange(&pc_front_end_trace_stop, 0);
        pc_front_end_trace_thread_handle = CreateThread(
            NULL, 0u, pc_front_end_trace_thread_main, NULL, 0u, NULL);
        if (pc_front_end_trace_thread_handle == NULL) {
            SudekiMpUninstallCleanroomMenu();
            return FALSE;
        }
    }
    SudekiMpLogFormat(
        "cleanroom_menu event=install status=success toggle_key=0x%02lx "
        "lead=PC_Ailish actor_policy=native_internal_spawn_and_remove "
        "dummy_resource=MON_TrainingDummy dummy_placement=cleanroom_center_anchor "
        "controls=combat_camera_infinite_sp_infinite_spirit "
        "resource_defaults=enabled multiplayer_integration=%s traversal=%s\r\n",
        (unsigned long)menu_toggle_key,
        integrated ? "external_control_and_render_hooks" :
            (roster ? "title_roster_hooks" :
                (traversal ? "world_aware_traversal" : "standalone_hooks")),
        traversal ? "true" : "false"
    );
    return TRUE;
}

BOOL SudekiMpInstallCleanroomMenu(HMODULE game_module, UINT toggle_key) {
    return install_cleanroom_menu_internal(
        game_module, toggle_key, FALSE, FALSE, FALSE, FALSE,
        FALSE, 0u, 1.0f);
}

BOOL SudekiMpInstallIntegratedCleanroomMenu(
    HMODULE game_module,
    UINT toggle_key
) {
    return install_cleanroom_menu_internal(
        game_module, toggle_key, TRUE, FALSE, FALSE, FALSE,
        FALSE, 0u, 1.0f);
}

BOOL SudekiMpInstallZoneTraversalMenu(
    HMODULE game_module,
    UINT toggle_key,
    BOOL skip_startup_movies
) {
    return install_cleanroom_menu_internal(
        game_module, toggle_key, FALSE, FALSE, skip_startup_movies, TRUE,
        FALSE, 0u, 1.0f);
}

BOOL SudekiMpInstallCoopRosterMenu(
    HMODULE game_module,
    UINT toggle_key,
    BOOL integrated_multiplayer,
    BOOL skip_startup_movies,
    BOOL enable_story_test_boost,
    UINT configured_story_test_boost_key,
    float configured_story_test_boost_multiplier
) {
    return install_cleanroom_menu_internal(
        game_module, toggle_key, integrated_multiplayer, TRUE,
        skip_startup_movies, FALSE,
        enable_story_test_boost,
        configured_story_test_boost_key,
        configured_story_test_boost_multiplier);
}

void SudekiMpUninstallCleanroomMenu(void) {
    HANDLE trace_thread = pc_front_end_trace_thread_handle;

    SudekiMpControlSeparationReportRoamingBoundaryOverlay(FALSE);
    InterlockedExchange(&pc_front_end_trace_stop, 1);
    pc_front_end_trace_thread_handle = NULL;
    if (trace_thread != NULL) {
        (void)WaitForSingleObject(trace_thread, 1000u);
        CloseHandle(trace_thread);
    }
    if (roster_native_page_state_active &&
        roster_native_page_controller != NULL) {
        native_roster_release_animated_rows();
        native_roster_release_options_rows(
            roster_native_page_controller);
        (void)native_roster_leave_page_state(
            roster_native_page_controller);
    }
    else {
        native_roster_release_animated_rows();
    }
    native_roster_restore_stock_labels();
    SudekiMpRestoreInlineHook(&native_save_entry_update_hook);
    native_save_entry_update_trampoline = NULL;
    SudekiMpRestoreInlineHook(&native_save_page_action_hook);
    native_save_page_action_trampoline = NULL;
    SudekiMpRestoreInlineHook(&native_save_page_input_hook);
    native_save_page_input_trampoline = NULL;
    native_save_entry_last_object = NULL;
    native_save_entry_last_mode = UINT_MAX;
    native_save_entry_last_selection = UINT_MAX;
    native_save_page_action_last_object = NULL;
    native_save_page_action_last_event = UINT_MAX;
    native_save_page_action_last_argument = UINT_MAX;
    native_save_page_input_last_object = NULL;
    native_save_page_input_last_event = UINT_MAX;
    native_save_page_input_last_argument = UINT_MAX;
    SudekiMpRestoreInlineHook(&front_end_action_hook);
    SudekiMpRestorePointerHook(&front_end_action_vtable_hook);
    SudekiMpRestorePointerHook(&front_end_render_hook);
    SudekiMpRestoreInlineHook(&front_end_update_hook);
    SudekiMpRestoreInlineHook(&front_end_state_update_hook);
    SudekiMpRestoreInlineHook(&startup_movie_hook);
    SudekiMpRestoreRelativeCallHook(&roster_ui_scene_render_hook);
    SudekiMpRestoreRelativeCallHook(&frame_end_hook);
    SudekiMpRestorePointerHook(&controller_update_hook);
    release_com_object(&menu_texture);
    release_com_object(&player_two_badge_texture);
    release_com_object(&transition_vote_texture);
    release_com_object(&roaming_boundary_texture);
    release_com_object(&blacksmith_ui_texture);
    release_com_object(&roster_button_texture);
    release_com_object(&roster_backdrop_texture);
    roster_release_native_portraits();
    menu_texture_device = NULL;
    player_two_badge_device = NULL;
    transition_vote_texture_device = NULL;
    roaming_boundary_texture_device = NULL;
    blacksmith_ui_texture_device = NULL;
    roster_button_texture_device = NULL;
    roster_backdrop_texture_device = NULL;
    player_two_badge_dirty = FALSE;
    transition_vote_texture_dirty = FALSE;
    roaming_boundary_texture_dirty = FALSE;
    transition_vote_overlay_failure_logged = FALSE;
    save_book_vote_overlay_failure_logged = FALSE;
    roaming_boundary_overlay_failure_logged = FALSE;
    blacksmith_ui_texture_dirty = FALSE;
    blacksmith_ui_overlay_failure_logged = FALSE;
    ZeroMemory(&blacksmith_ui_last_snapshot,
        sizeof(blacksmith_ui_last_snapshot));
    transition_vote_overlay_serial = 0u;
    transition_vote_overlay_tenth_seconds = 0u;
    transition_vote_overlay_state = 0u;
    transition_vote_overlay_kind = 0u;
    transition_vote_overlay_participant_mask = 0u;
    transition_vote_overlay_accepted_mask = 0u;
    ZeroMemory(transition_vote_overlay_destination,
        sizeof(transition_vote_overlay_destination));
    reset_save_book_vote_input();
    roaming_boundary_overlay_phase = 0u;
    roaming_boundary_overlay_percent = 0u;
    roster_button_texture_dirty = FALSE;
    original_controller_update = NULL;
    original_frame_end = NULL;
    original_front_end_action = NULL;
    original_front_end_update = NULL;
    original_front_end_render = NULL;
    original_ui_scene_render = NULL;
    original_front_end_state_update = NULL;
    original_movie_play = NULL;
    front_end_menu_builder = NULL;
    front_end_selection_refresh = NULL;
    roster_native_screen = FALSE;
    roster_native_screen_kind = NATIVE_ROSTER_NONE;
    roster_native_selection = 0u;
    SudekiMpTalosCoopBalanceReset();
    roster_confirm_input_armed = FALSE;
    roster_native_transition_started = 0u;
    roster_native_transition_from_title = FALSE;
    roster_native_page_contract_ready = FALSE;
    roster_native_page_state_active = FALSE;
    roster_native_page_takeover_pending = FALSE;
    roster_native_page_leave_requested = FALSE;
    roster_native_page_leave_started_ms = 0u;
    roster_native_page_controller = NULL;
    roster_native_page_saved_active = NULL;
    roster_native_page_saved_backing = NULL;
    roster_native_page_saved_state = 0u;
    roster_native_page_saved_previous = 0u;
    roster_native_page_saved_mode = 0u;
    ZeroMemory(roster_native_options_rows,
        sizeof(roster_native_options_rows));
    roster_native_options_rows_bound = FALSE;
    ZeroMemory(roster_native_page_object,
        sizeof(roster_native_page_object));
    ZeroMemory(roster_native_page_vtable,
        sizeof(roster_native_page_vtable));
    game_base = NULL;
    d3d_device_global = NULL;
    menu_toggle_key = 0u;
    story_test_boost_enabled = FALSE;
    story_test_boost_active = FALSE;
    story_test_boost_runtime_applied = FALSE;
    story_test_boost_key_was_down = FALSE;
    story_test_boost_failure_logged = FALSE;
    story_test_boost_key = 0u;
    story_test_boost_multiplier = 1.0f;
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
    infinite_jetpack_fuel = FALSE;
    infinite_jetpack_fuel_valid = FALSE;
    integrated_multiplayer_mode = FALSE;
    zone_traversal_mode = FALSE;
    zone_traversal_page = ZONE_TRAVERSAL_PAGE_WORLDS;
    zone_traversal_selection = 0u;
    zone_traversal_waiting = FALSE;
    zone_traversal_waiting_since = 0u;
    zone_traversal_waiting_world[0] = '\0';
    roster_mode = FALSE;
    roster_locked = FALSE;
    loaded_save_coop_autostart_enabled = FALSE;
    loaded_save_coop_autostart_roster_published = FALSE;
    loaded_save_coop_autostart_last_locked = FALSE;
    roster_coop_profile = FALSE;
    roster_waiting_new_game = FALSE;
    roster_replaying_new_game = FALSE;
    roster_resume_committed = FALSE;
    roster_resume_controller = NULL;
    roster_pending_controller = NULL;
    roster_original_menu_valid = FALSE;
    roster_original_menu_count = 0u;
    roster_original_menu_selection = 0u;
    roster_persistence_path[0] = '\0';
    roster_player_one = SUDEKIMP_CLEANROOM_AILISH;
    roster_player_two = SUDEKIMP_CLEANROOM_TAL;
    roster_cursor = 0u;
    multiplayer_requested = FALSE;
    multiplayer_active = FALSE;
    multiplayer_input_ready = FALSE;
    multiplayer_participation_requested = FALSE;
    coop_role_lock_active = FALSE;
    coop_selected_actor = SUDEKIMP_CLEANROOM_TAL;
    coop_ready_failed_until = 0u;
    coop_lobby_prompted = FALSE;
    last_status_update = 0u;
    SudekiMpCleanroomEngineReset();
}
