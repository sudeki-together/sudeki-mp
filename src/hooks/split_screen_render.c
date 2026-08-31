#include "hooks/split_screen_render.h"

#include "engine/log.h"
#include "engine/local_viewport_layout.h"
#include "engine/orbit_camera.h"
#include "engine/player_combat_context.h"
#include "engine/player_statehood.h"
#include "hooks/call_hook.h"
#include "hooks/control_separation.h"
#include "input/bridge_protocol.h"
#include "input/bridge_receiver.h"
#include "input/local_input_hub.h"

#include <math.h>
#include <limits.h>
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
typedef BOOL (*QuickMenuIsActiveFunction)(void);
typedef uint8_t (*QuickMenuCloseFunction)(void);
typedef void (*QuickMenuStartFunction)(void);
/* UIBlackSmithActive returns only AL and leaves the upper EAX bits intact. */
typedef uint8_t (*NativeModalIsActiveFunction)(void);
typedef void (*HudPartyPointerCopyFunction)(void);
#if defined(__GNUC__) && defined(__i386__)
#define SUDEKIMP_THISCALL __attribute__((thiscall))
#endif
typedef void (SUDEKIMP_THISCALL *QuickMenuRenderSubmitFunction)(
    void *quick_menu
);
typedef uint8_t (SUDEKIMP_THISCALL *QuickMenuInputFunction)(
    void *quick_menu,
    unsigned int event_kind,
    unsigned int command,
    unsigned int value
);
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
typedef void (SUDEKIMP_THISCALL *CameraManagerSetCameraTargetFunction)(
    void *manager,
    const char *name,
    void *gel_pointer
);
typedef void *(SUDEKIMP_THISCALL *GroupPlayersGetPlayerGroupFunction)(
    void *group_players,
    int position
);
typedef uint8_t (*GelPointerResolveEntityFunction)(
    void *gel_pointer,
    void *tracked_entity
);
typedef void (SUDEKIMP_THISCALL *TrackedEntityCleanupFunction)(
    void *tracked_entity
);
typedef int (SUDEKIMP_THISCALL *CameraManagerGetCameraModeFunction)(
    void *manager,
    const char *name
);
typedef void (SUDEKIMP_THISCALL *CameraManagerSetCameraStateFunction)(
    void *manager,
    const char *name,
    const char *state,
    float transition_a,
    BOOL transition_flag,
    float transition_b
);
typedef void (SUDEKIMP_THISCALL *CameraInputEventFunction)(
    void *camera,
    const void *event
);
typedef BOOL (SUDEKIMP_THISCALL *CameraManagerSetRenderCameraFunction)(
    void *manager,
    const char *name
);
typedef void (SUDEKIMP_THISCALL *GameSpeedSetModeFunction)(
    void *game_speed,
    int mode
);
typedef void (SUDEKIMP_THISCALL *MotionBlurPostRenderFunction)(
    void *callback,
    unsigned char flags
);
typedef void (SUDEKIMP_THISCALL *ScreenshotPostRenderFunction)(
    void *callback,
    unsigned char flags
);
typedef void *(*NativeHistoryResourceFactoryFunction)(void);
typedef unsigned int (SUDEKIMP_THISCALL *ModelAnimationCountFunction)(
    void *model_interface
);
typedef int (SUDEKIMP_THISCALL *ModelAnimationLookupFunction)(
    void *model_interface,
    int animation_handle
);
typedef int (SUDEKIMP_THISCALL *ModelAnimationSelectorGetFunction)(
    void *model_interface,
    int channel,
    unsigned int submodel
);
typedef void (SUDEKIMP_THISCALL *ModelAnimationSelectorSetFunction)(
    void *model_interface,
    int channel,
    unsigned int submodel,
    int selector
);
typedef float (SUDEKIMP_THISCALL *ModelAnimationValueGetFunction)(
    void *model_interface,
    int channel,
    unsigned int submodel
);
typedef void (SUDEKIMP_THISCALL *ModelAnimationValueSetFunction)(
    void *model_interface,
    int channel,
    unsigned int submodel,
    float value
);
typedef void (SUDEKIMP_THISCALL *ModelAnimationTimeSetFunction)(
    void *model_interface,
    int channel,
    unsigned int submodel,
    float value,
    int force
);
typedef unsigned char (SUDEKIMP_THISCALL *ModelAnimationStateGetFunction)(
    void *model_interface,
    int channel,
    unsigned int submodel
);
typedef void (SUDEKIMP_THISCALL *ModelAnimationStateSetFunction)(
    void *model_interface,
    int channel,
    unsigned int submodel,
    int enabled
);
typedef float (SUDEKIMP_THISCALL *ModelAnimationBlendGetFunction)(
    void *model_interface,
    int channel
);
typedef void (SUDEKIMP_THISCALL *ModelAnimationBlendSetFunction)(
    void *model_interface,
    int channel,
    float blend
);
typedef unsigned char (SUDEKIMP_THISCALL *GroupPlayersInCombatFunction)(
    const void *group_players
);
typedef const float *(SUDEKIMP_THISCALL *RenderLocatorMatrixFunction)(
    void *provider,
    int locator_index
);

enum {
    RVA_D3D_DEVICE_GLOBAL = 0x003c31dcu,
    RVA_SCENE_MANAGER_GLOBAL = 0x00408d58u,
    RVA_PC_QUIT_SCREEN_GLOBAL = 0x00408d68u,
    RVA_SPIRIT_STRIKE_MANAGER_GLOBAL = 0x00408d30u,
    RVA_GAMEPLAY_HUD_GLOBAL = 0x003c2f9cu,
    RVA_WORLD_GLOBAL = 0x00408d10u,
    RVA_WORLD_SCENE_GLOBAL = 0x00408d1cu,
    RVA_ACTIVE_GROUP_GLOBAL = 0x00408d94u,
    RVA_CHARACTER_CONTROLLER_GLOBAL = 0x00408da4u,
    RVA_CHARACTER_SWITCH_UI_GATE_GLOBAL = 0x00408d3cu,
    RVA_CHARACTER_SWITCH_INPUT_BLOCK_GLOBAL = 0x00408db8u,
    RVA_GAME_CAMERA_MODE_GLOBAL = 0x00408da8u,
    RVA_CAMERA_MANAGER_GLOBAL = 0x00409d7cu,
    RVA_CAMERA_MANAGER_ADD_CAMERA = 0x00036c10u,
    RVA_CAMERA_MANAGER_REMOVE_CAMERA = 0x00036de0u,
    RVA_CAMERA_MANAGER_GET_CAMERA = 0x00036ed0u,
    RVA_CAMERA_MANAGER_SET_RENDER_CAMERA = 0x00036fb0u,
    RVA_CAMERA_MANAGER_SET_CAMERA_TARGET = 0x00037170u,
    RVA_CAMERA_MANAGER_GET_CAMERA_MODE = 0x000374b0u,
    RVA_CAMERA_MANAGER_SET_CAMERA_STATE = 0x00037530u,
    RVA_CAMERA_INPUT_EVENT = 0x000e85f0u,
    RVA_CAMERA_INPUT_EVENT_VTABLE_SLOT = 0x002cce5cu,
    RVA_GAME_SPEED_SET_MODE = 0x00207560u,
    RVA_FIXED_ALTERNATE_SPEED = 0x002c4018u,
    RVA_MOTION_BLUR_POST_RENDER = 0x001de0b0u,
    RVA_SCREENSHOT_POST_RENDER = 0x001de7b0u,
    RVA_HISTORY_RESOURCE_FACTORY = 0x001f6c70u,
    RVA_MOTION_BLUR_POST_RENDER_VTABLE_SLOT = 0x002dd930u,
    RVA_SCREENSHOT_POST_RENDER_VTABLE = 0x002dd90cu,
    RVA_SCREENSHOT_POST_RENDER_VTABLE_SLOT = 0x002dd910u,
    RVA_POSITION_SET_FORWARD = 0x001114d0u,
    RVA_GROUP_PLAYERS_IN_COMBAT = 0x00004fa0u,
    RVA_GROUP_PLAYERS_GET_PLAYER_GROUP = 0x000246d0u,
    RVA_GEL_POINTER_RESOLVE_ENTITY = 0x001bf4e0u,
    RVA_TRACKED_ENTITY_CLEANUP = 0x000015e0u,
    RVA_GEL_GROUP_PTR_DELETING_DESTRUCTOR = 0x00001b30u,
    RVA_GEL_GROUP_PTR_VTABLE = 0x002c0098u,
    RVA_GEL_GROUP_PTR_GET_RAW_ENTITY = 0x000017b0u,
    RVA_GEL_GROUP_PTR_TYPE_NAME = 0x00001820u,
    RVA_GEL_POINTER_RESOLVER_HANDLER = 0x002947f8u,
    RVA_GROUP_PLAYERS_PREVIOUS_CHARACTER = 0x00023f60u,
    RVA_GROUP_PLAYERS_NEXT_CHARACTER = 0x00024060u,
    RVA_INGAME_UI_CONTROLLER_GLOBAL = 0x003c2f88u,
    RVA_SHOP_LAYER_GLOBAL = 0x003c2f70u,
    RVA_BLACKSMITH_LAYER_GLOBAL = 0x003c2f74u,
    RVA_SHOP_IS_ACTIVE = 0x0008d1c0u,
    RVA_BLACKSMITH_IS_ACTIVE = 0x00092c60u,
    RVA_INGAME_UI_CONTROLLER_VTABLE = 0x002caf9cu,
    RVA_SHOP_LAYER_VTABLE = 0x002cabb4u,
    RVA_BLACKSMITH_LAYER_VTABLE = 0x002cacfcu,
    RVA_INGAME_UI_CONTROLLER_UPDATE = 0x0009d1d0u,
    RVA_INGAME_UI_CONTROLLER_RENDER = 0x0009d8d0u,
    RVA_INGAME_UI_CONTROLLER_INPUT = 0x0009c930u,
    RVA_SHOP_LAYER_UPDATE = 0x00089660u,
    RVA_SHOP_LAYER_RENDER = 0x0008a210u,
    RVA_SHOP_LAYER_INPUT = 0x000898a0u,
    RVA_SHOP_LAYER_RESOURCE_CREATE = 0x0008c850u,
    RVA_SHOP_LAYER_RESOURCE_DESTROY = 0x0008d030u,
    RVA_BLACKSMITH_LAYER_UPDATE = 0x0008d6f0u,
    RVA_BLACKSMITH_LAYER_RENDER = 0x0008e910u,
    RVA_BLACKSMITH_LAYER_INPUT = 0x0008d970u,
    RVA_BLACKSMITH_LAYER_RESOURCE_CREATE = 0x00090c20u,
    RVA_BLACKSMITH_LAYER_RESOURCE_DESTROY = 0x00091b40u,
    RVA_QUICK_MENU_IS_ACTIVE = 0x0009c330u,
    RVA_QUICK_MENU_CLOSE = 0x0009c360u,
    RVA_QUICK_MENU_START = 0x0009c3a0u,
    RVA_QUICK_MENU_GLOBAL = 0x003c2f84u,
    RVA_PC_QUIT_SCREEN_SHOW = 0x0001dbe0u,
    RVA_PC_QUIT_SCREEN_RENDER = 0x0001d690u,
    RVA_PC_QUIT_SCREEN_RENDER_CALL = 0x0028d572u,
    RVA_QUICK_MENU_RENDER_SUBMIT = 0x0009bba0u,
    RVA_QUICK_MENU_RENDER_SUBMIT_VTABLE_SLOT = 0x002caf28u,
    RVA_QUICK_MENU_VTABLE = 0x002caf1cu,
    RVA_QUICK_MENU_INPUT = 0x00098b40u,
    RVA_QUICK_MENU_INPUT_VTABLE_SLOT = 0x002caf48u,
    RVA_QUICK_MENU_NATIVE_TOGGLE = 0x0000a080u,
    RVA_QUICK_MENU_NATIVE_TOGGLE_CALL = 0x00028228u,
    RVA_QUICK_MENU_OWNER_COPY_UI_ACTIVE = 0x0000aff1u,
    RVA_QUICK_MENU_OWNER_COPY_SELECTION = 0x00099341u,
    RVA_QUICK_MENU_OWNER_COPY_SELECTION_WEAPON = 0x000995d6u,
    RVA_QUICK_MENU_OWNER_COPY_SELECTION_ITEM = 0x000996e9u,
    RVA_QUICK_MENU_OWNER_COPY_SELECTION_SKILL = 0x0009984bu,
    RVA_QUICK_MENU_OWNER_COPY_DETAIL_HEADER = 0x00099afcu,
    RVA_QUICK_MENU_OWNER_COPY_DETAIL_SKILL = 0x00099e80u,
    RVA_QUICK_MENU_OWNER_COPY_DETAIL_WEAPON = 0x00099f48u,
    RVA_QUICK_MENU_OWNER_COPY_DETAIL_ITEM = 0x00099f8bu,
    RVA_QUICK_MENU_OWNER_COPY_REBUILD_HEADER = 0x0009a33eu,
    RVA_QUICK_MENU_OWNER_COPY_REBUILD_RESOURCE = 0x0009a960u,
    RVA_QUICK_MENU_OWNER_COPY_REBUILD_SKILL = 0x0009b40cu,
    RVA_QUICK_MENU_OWNER_COPY_REBUILD_WEAPON = 0x0009b7cbu,
    RVA_QUICK_MENU_OWNER_COPY_REBUILD_ITEM = 0x0009cc15u,
    RVA_QUICK_MENU_OWNER_COPY_DEFAULT_RECIPIENT = 0x0009c153u,
    RVA_HUD_PARTY_POINTER_COPY = 0x000015b0u,
    RVA_HUD_GROUP_VALUES_POINTER_CALL = 0x00181517u,
    RVA_HUD_GIZMO_PORTRAIT_POINTER_CALL = 0x000aab3au,
    RVA_HUD_GIZMO_VALUES_POINTER_CALL = 0x000a9d5bu,
    RVA_HUD_GIZMO_NAME_POINTER_CALL = 0x000a9e15u,
    RVA_HUD_GIZMO_STATUS_POINTER_CALL = 0x000aacabu,
    RVA_MINIMAP_UPDATE_POINTER_CALL = 0x00087760u,
    RVA_MINIMAP_RENDER_POINTER_CALL = 0x00087af7u,
    RVA_CHARACTER_TYPE_TO_PORTRAIT_ENUM = 0x0003f430u,
    RVA_HUD_PORTRAIT_RESOURCE_SELECT = 0x0015c070u,
    RVA_HUD_PORTRAIT_RESOURCE_INDEX_TABLE = 0x002c2a94u,
    RVA_HUD_PORTRAIT_GIZMO_VTABLE = 0x002cb590u,
    RVA_CHARACTER_TYPE_TO_PORTRAIT_LOOKUP = 0x0003f498u,
    RVA_UI_RESOURCE_TABLE_INITIALIZED = 0x003c2fefu,
    RVA_ANIMATION_RENDERER_VTABLE = 0x002df8ecu,
    RVA_ANIMATION_RENDERER_LOOKUP = 0x0021bac0u,
    RVA_ANIMATION_RENDERER_COUNT = 0x0021bb10u,
    RVA_ANIMATION_RENDERER_SELECTOR_SET = 0x00223000u,
    RVA_ANIMATION_RENDERER_SELECTOR_GET = 0x002230b0u,
    RVA_ANIMATION_RENDERER_RATE_SET = 0x002230d0u,
    RVA_ANIMATION_RENDERER_RATE_GET = 0x00223160u,
    RVA_ANIMATION_RENDERER_TIME_SET = 0x00223180u,
    RVA_ANIMATION_RENDERER_TIME_GET = 0x00223220u,
    RVA_ANIMATION_RENDERER_STATE_SET = 0x00223240u,
    RVA_ANIMATION_RENDERER_STATE_GET = 0x00223290u,
    RVA_ANIMATION_RENDERER_BLEND_SET = 0x002234c0u,
    RVA_ANIMATION_RENDERER_BLEND_GET = 0x002234e0u,
    RVA_RENDER_LOCATOR_INDEX = 0x000c5ff0u,
    RVA_RANGED_WEAPON_REATTACH = 0x000d8280u,
    RVA_RENDER_START = 0x001dce30u,
    RVA_RENDER_START_CALL = 0x0028d443u,
    RVA_FRAME_END = 0x001dd540u,
    RVA_FRAME_END_CALL = 0x0028d58cu,
    PARTY_SLOT_COUNT = 4u,
    PARTY_SLOT_ZERO_OFFSET = 0x90u,
    PARTY_SLOT_STRIDE = 0x0cu,
    QUICK_MENU_OWNER_COPY_HOOK_COUNT = 14u,
    QUICK_MENU_ACTIVE_OFFSET = 0x29u,
    QUICK_MENU_DEFAULT_RECIPIENT_OFFSET = 0x1d8u,
    QUICK_MENU_CATEGORY_OFFSET = 0x204u,
    QUICK_MENU_SKILLS_CATEGORY = 0u,
    QUICK_MENU_INPUT_EVENT_DOWN = 5u,
    QUICK_MENU_INPUT_EVENT_UP = 6u,
    QUICK_MENU_INPUT_EVENT_POINTER = 0x19u,
    QUICK_MENU_COMMAND_CONFIRM = 0u,
    QUICK_MENU_COMMAND_CANCEL = 11u,
    QUICK_MENU_COMMAND_UP = 6u,
    QUICK_MENU_COMMAND_DOWN = 7u,
    CAMERA_RENDER_STATE_OFFSET = 0x34u,
    CAMERA_ACTIVE_STATE_OFFSET = 0x3cu,
    CAMERA_ACTIVE_STATE_DATA_OFFSET = 0x40u,
    CAMERA_POSITION_TARGET_OFFSET = 0xb4u,
    CAMERA_LOOK_AT_TARGET_OFFSET = 0xb8u,
    RENDER_STATE_GENERATION_OFFSET = 0x2cu,
    CAMERA_MODE_EXPLORATION = 0,
    CAMERA_MODE_STATE_TRANSITION = 0x13,
    NATIVE_CAMERA_READINESS_FRAME_LIMIT = 240u,
    CONTROLLER_MODE_80_OFFSET = 0x80u,
    CONTROLLER_MODE_84_OFFSET = 0x84u,
    CONTROLLER_TARGET_OFFSET = 0x248u,
    CONTROLLER_NEXT_CHARACTER_OFFSET = 0xf4u,
    CONTROLLER_PREVIOUS_CHARACTER_OFFSET = 0xfcu,
    INGAME_UI_CONTROLLER_SHOP_OFFSET = 0x74u,
    INGAME_UI_CONTROLLER_BLACKSMITH_OFFSET = 0x78u,
    INGAME_UI_CONTROLLER_CURRENT_MODE_OFFSET = 0xb8u,
    INGAME_UI_CONTROLLER_NEXT_MODE_OFFSET = 0xbcu,
    SHOP_LAYER_STATE_OFFSET = 0x88u,
    BLACKSMITH_LAYER_ACTIVE_OFFSET = 0x29u,
    BLACKSMITH_LAYER_STATE_OFFSET = 0x314u,
    CHARACTER_SWITCH_FLAGS_50_OFFSET = 0x50u,
    CHARACTER_SWITCH_FLAGS_60_OFFSET = 0x60u,
    CHARACTER_SWITCH_BUSY_FLAGS_50 = 0x081802c0u,
    CHARACTER_SWITCH_BUSY_FLAGS_60 = 0x00000001u,
    CHARACTER_SWITCH_UI_GATE_OFFSET = 0x47du,
    PARTY_COUNT_OFFSET = 0xccu,
    PARTY_STATE_D0_OFFSET = 0xd0u,
    PARTY_SWITCHING_D6_OFFSET = 0xd6u,
    PARTY_STATE_D7_OFFSET = 0xd7u,
    COOP_ROSTER_READY_WINDOW_MS = 250u,
    GAME_CAMERA_POINTER_OFFSET = 0x0cu,
    PC_QUIT_SCREEN_VISIBLE_OFFSET = 0x1c2u,
    GAMEPLAY_HUD_PORTRAIT_GIZMO_ARRAY_OFFSET = 0x138u,
    HUD_PORTRAIT_ENUM_OFFSET = 0x2a8u,
    HUD_PORTRAIT_CYCLE_ICON_OFFSET = 0x2cu,
    HUD_PORTRAIT_PARTY_INDEX_OFFSET = 0x32cu,
    CHARACTER_RESOURCE_OBJECT_OFFSET = 0x2cu,
    RANGED_ANIMATION_CHANNEL_COUNT = 5u,
    RANGED_MODEL_RENDER_SWAP_COUNT = 2u,
    RANGED_FIRST_PERSON_ARBITER_FLAG = 0x00400000u,
    RENDER_OBJECT_HIDDEN_FLAG = 0x00000004u,
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
    D3D_MULTISAMPLE_NONE = 0,
    FIXED_THREE_SEAT_COUNT = 3u,
    FIXED_THREE_COMPANION_FIRST_SEAT = 1u,
    FIXED_THREE_COMPANION_LAST_SEAT = 2u,
    FIXED_THREE_HUMAN_MASK = 0x07u
};

typedef enum SudekiMpQuickMenuSessionPhase {
    SUDEKIMP_QUICK_MENU_SESSION_IDLE = 0,
    SUDEKIMP_QUICK_MENU_SESSION_OPEN_REQUESTED,
    SUDEKIMP_QUICK_MENU_SESSION_NATIVE_OPEN,
    SUDEKIMP_QUICK_MENU_SESSION_CLOSING,
    SUDEKIMP_QUICK_MENU_SESSION_QUARANTINED
} SudekiMpQuickMenuSessionPhase;

typedef struct SudekiMpQuickMenuSession {
    SudekiMpQuickMenuSessionPhase phase;
    unsigned int owner_seat;
    uint32_t serial;
    void *actor;
    uint32_t actor_generation;
    void *input_identity;
    uint32_t input_generation;
    void *camera;
    void *render_state;
    void *native_menu;
    void *group;
    uint32_t held_party_source[3];
    BOOL quarantine_logged;
} SudekiMpQuickMenuSession;

typedef struct SudekiMpFixedThreeSeatRuntime {
    BOOL configured;
    BOOL transaction_active;
    BOOL presentation_clear_this_frame;
    BOOL cameras_acquired;
    BOOL compositor_logged;
    BOOL rejection_logged;
    void *manager;
    void *group;
    void *host_controller;
    void *actors[FIXED_THREE_SEAT_COUNT];
    uint32_t actor_generations[FIXED_THREE_SEAT_COUNT];
    unsigned int party_slots[FIXED_THREE_SEAT_COUNT];
    const void *input_identities[FIXED_THREE_SEAT_COUNT];
    uint32_t input_generations[FIXED_THREE_SEAT_COUNT];
    void *cameras[FIXED_THREE_SEAT_COUNT];
    void *render_states[FIXED_THREE_SEAT_COUNT];
    BOOL camera_transform_initialized[FIXED_THREE_SEAT_COUNT];
    float camera_last_target[FIXED_THREE_SEAT_COUNT][3];
    float camera_pitch_offset[FIXED_THREE_SEAT_COUNT];
    DWORD camera_input_last_tick[FIXED_THREE_SEAT_COUNT];
    BOOL camera_input_logged[FIXED_THREE_SEAT_COUNT];
    void *frame_surfaces[FIXED_THREE_SEAT_COUNT];
    void *frame_textures[FIXED_THREE_SEAT_COUNT];
    void *frame_device;
    SudekiMpD3DSurfaceDesc frame_description;
    unsigned int frame_valid_mask;
    unsigned int frame_owner_evidence_mask;
    SudekiMpLocalViewportLayout layout;
    uint8_t scheduled_seat;
    uint8_t rendered_seat;
    unsigned int hud_evidence_seat;
    unsigned int hud_role_mask;
    BOOL hud_source_failure_this_frame;
    unsigned int portrait_evidence_seat;
    unsigned int portrait_role_mask;
    uint8_t minimap_expected_update_seat;
    uint32_t minimap_expected_update_epoch;
    uint8_t minimap_update_seat;
    uint32_t minimap_update_epoch;
    BOOL minimap_update_valid;
    uint8_t minimap_render_seat;
    BOOL minimap_render_valid;
    void **render_swap_slot;
    void *render_swap_native_state;
    void *render_swap_applied_state;
    BOOL render_swap_active;
} SudekiMpFixedThreeSeatRuntime;

typedef struct SudekiMpFixedThreeSeatLeaseCandidate {
    void *group;
    void *host_controller;
    void *actors[FIXED_THREE_SEAT_COUNT];
    uint32_t actor_generations[FIXED_THREE_SEAT_COUNT];
    unsigned int party_slots[FIXED_THREE_SEAT_COUNT];
    const void *input_identities[FIXED_THREE_SEAT_COUNT];
    uint32_t input_generations[FIXED_THREE_SEAT_COUNT];
} SudekiMpFixedThreeSeatLeaseCandidate;

static const uint32_t quick_menu_owner_copy_call_rvas[
    QUICK_MENU_OWNER_COPY_HOOK_COUNT
] = {
    RVA_QUICK_MENU_OWNER_COPY_UI_ACTIVE,
    RVA_QUICK_MENU_OWNER_COPY_SELECTION,
    RVA_QUICK_MENU_OWNER_COPY_SELECTION_WEAPON,
    RVA_QUICK_MENU_OWNER_COPY_SELECTION_ITEM,
    RVA_QUICK_MENU_OWNER_COPY_SELECTION_SKILL,
    RVA_QUICK_MENU_OWNER_COPY_DETAIL_HEADER,
    RVA_QUICK_MENU_OWNER_COPY_DETAIL_SKILL,
    RVA_QUICK_MENU_OWNER_COPY_DETAIL_WEAPON,
    RVA_QUICK_MENU_OWNER_COPY_DETAIL_ITEM,
    RVA_QUICK_MENU_OWNER_COPY_REBUILD_HEADER,
    RVA_QUICK_MENU_OWNER_COPY_REBUILD_RESOURCE,
    RVA_QUICK_MENU_OWNER_COPY_REBUILD_SKILL,
    RVA_QUICK_MENU_OWNER_COPY_REBUILD_WEAPON,
    RVA_QUICK_MENU_OWNER_COPY_REBUILD_ITEM
};

typedef struct SudekiMpAnimationTraceSnapshot {
    BOOL valid;
    void *character;
    void *component;
    void *weapon;
    void *weapon_item;
    void *attached_wrapper;
    void *first_person_wrapper;
    void *world_wrapper;
    void *first_person_render_object;
    void *world_render_object;
    uint32_t first_person_render_flags;
    uint32_t world_render_flags;
    unsigned int first_person_active;
    unsigned int animation_bank;
    unsigned int animation_ids[RANGED_ANIMATION_CHANNEL_COUNT];
    int first_person_selectors[RANGED_ANIMATION_CHANNEL_COUNT];
    int world_selectors[RANGED_ANIMATION_CHANNEL_COUNT];
    unsigned int first_person_states[RANGED_ANIMATION_CHANNEL_COUNT];
    unsigned int world_states[RANGED_ANIMATION_CHANNEL_COUNT];
} SudekiMpAnimationTraceSnapshot;

typedef struct SudekiMpRangedModelRenderSwap {
    BOOL active;
    void *character;
    void *position;
    void *component;
    void *first_person_render_object;
    void *world_render_object;
    void *weapon;
    BOOL weapon_reattached;
    BOOL transform_trace_sample;
    unsigned int transform_trace_sequence;
    BOOL saved_weapon_local_matrix_valid;
    float saved_weapon_local_matrix[16];
    void **attachment_slot;
    void *applied_attachment;
    void *saved_attachment;
    uint32_t *first_person_flags;
    uint32_t applied_first_person_flags;
    uint32_t saved_first_person_flags;
    uint32_t *world_flags;
    uint32_t applied_world_flags;
    uint32_t saved_world_flags;
} SudekiMpRangedModelRenderSwap;

typedef struct SudekiMpPlayerTwoCollisionSelfCull {
    BOOL active;
    void *character;
    void *world_render_object;
    uint32_t *world_flags;
    uint32_t saved_world_flags;
    uint32_t applied_world_flags;
} SudekiMpPlayerTwoCollisionSelfCull;

/* Native CCamera::InputEvent (RVA 0xe85f0) reads only these fields from its
 * input record: action at +0x00, float magnitude at +0x0c, and the optional
 * event owner at +0x14.  Its own per-frame producer emits 0x69/0x6a/0x6c/0x6b
 * through this same shape. */
typedef struct SudekiMpNativeCameraInputEvent {
    uint32_t action;
    uint32_t reserved_04;
    uint32_t reserved_08;
    float magnitude;
    uint32_t reserved_10;
    uint16_t owner;
    uint16_t reserved_16;
} SudekiMpNativeCameraInputEvent;

typedef struct SudekiMpRangedWorldCompositorState {
    BOOL owned;
    BOOL applied;
    BOOL write_attempted;
    BOOL moving;
    BOOL standard_source_present;
    BOOL shock_source_present;
    BOOL action_playing;
    void *character;
    void *component;
    void *first_person_wrapper;
    void *world_wrapper;
    void *world_renderer;
    unsigned int submodels;
    int idle_selector;
    int walk_selector;
    int run_selector;
    int standard_action_selector;
    int shock_action_selector;
    unsigned int action_source_id;
    unsigned int action_target_id;
    int action_selector;
    DWORD action_started_tick;
    const char *last_rejection;
} SudekiMpRangedWorldCompositorState;

typedef struct SudekiMpRangedWorldAnimationMethods {
    ModelAnimationCountFunction count;
    ModelAnimationSelectorSetFunction set_selector;
    ModelAnimationSelectorGetFunction get_selector;
    ModelAnimationValueSetFunction set_rate;
    ModelAnimationValueGetFunction get_rate;
    ModelAnimationTimeSetFunction set_time;
    ModelAnimationValueGetFunction get_time;
    ModelAnimationStateSetFunction set_state;
    ModelAnimationStateGetFunction get_state;
    ModelAnimationBlendSetFunction set_blend;
    ModelAnimationBlendGetFunction get_blend;
} SudekiMpRangedWorldAnimationMethods;

static SudekiMpRelativeCallHook render_start_hook;
static SudekiMpRelativeCallHook frame_end_hook;
static SudekiMpRelativeCallHook quit_screen_render_hook;
static SudekiMpRelativeCallHook hud_group_values_pointer_hook;
static SudekiMpRelativeCallHook hud_gizmo_values_pointer_hook;
static SudekiMpRelativeCallHook hud_gizmo_name_pointer_hook;
static SudekiMpRelativeCallHook hud_gizmo_status_pointer_hook;
static SudekiMpRelativeCallHook minimap_update_pointer_hook;
static SudekiMpRelativeCallHook minimap_render_pointer_hook;
static SudekiMpRelativeCallHook quick_menu_owner_copy_hooks[
    QUICK_MENU_OWNER_COPY_HOOK_COUNT
];
static SudekiMpRelativeCallHook quick_menu_owner_default_recipient_hook;
static SudekiMpRelativeCallHook quick_menu_native_toggle_hook;
static SudekiMpInlineHook set_render_camera_hook;
static SudekiMpInlineHook set_game_speed_mode_hook;
static SudekiMpPointerHook motion_blur_post_render_hook;
static SudekiMpPointerHook screenshot_post_render_hook;
static SudekiMpPointerHook quick_menu_render_submit_hook;
static SudekiMpPointerHook quick_menu_input_hook;
static SudekiMpPointerHook camera_input_event_hook;
static uint8_t *game_base;
static RenderStartFunction original_render_start;
static FrameEndFunction original_frame_end;
static QuitScreenRenderFunction original_quit_screen_render
    __attribute__((used));
static QuickMenuRenderSubmitFunction original_quick_menu_render_submit;
static QuickMenuIsActiveFunction quick_menu_is_active;
static QuickMenuCloseFunction quick_menu_close;
static QuickMenuStartFunction quick_menu_start;
static QuickMenuInputFunction original_quick_menu_input;
static void *original_quick_menu_native_toggle __attribute__((used));
static TrackedEntityCleanupFunction quick_menu_party_pointer_cleanup;
static NativeModalIsActiveFunction shop_is_active;
static NativeModalIsActiveFunction blacksmith_is_active;
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
static BOOL minimap_ownership_logged[FIXED_THREE_SEAT_COUNT];
static BOOL minimap_owner_mismatch_logged;
static BOOL hud_mapping_trace_valid[FIXED_THREE_SEAT_COUNT][2];
static unsigned int hud_mapping_trace_resolved_slot[
    FIXED_THREE_SEAT_COUNT
][2];
static int gameplay_gate_last_state = -1;
static int shared_menu_gate_last_state = -1;
static unsigned int shared_interaction_modal_observation =
    SUDEKIMP_SHARED_INTERACTION_MODAL_NONE;
/* Set by the save-book action hook before it publishes a vote or invokes the
 * native continuation.  The atomic signal is consulted before any native UI
 * singleton inspection so construction itself is a fail-closed full-width
 * phase, not a one-frame blind spot. */
static volatile LONG native_save_modal_opening;
/* MoviePlay is a blocking native presentation takeover.  Its hook enters this
 * gate before invoking retail code so no named companion camera or cached
 * render surface can outlive the gameplay render loop that owns it. */
static volatile LONG native_movie_gate_depth;
static BOOL shared_interaction_modal_recovery_pending;
static BOOL shared_interaction_modal_had_live_split;
static SudekiMpInteractionKind shared_interaction_modal_published_kind =
    SUDEKIMP_INTERACTION_NONE;
static BOOL split_screen_render_installed;
static BOOL second_player_camera_enabled;
static BOOL dual_camera_frame_cache_enabled;
static UINT second_player_camera_virtual_key;
static BOOL second_player_camera_key_was_down;
static CameraManagerAddCameraFunction camera_manager_add_camera;
static CameraManagerRemoveCameraFunction camera_manager_remove_camera;
static CameraManagerGetCameraFunction camera_manager_get_camera;
static CameraManagerSetCameraTargetFunction camera_manager_set_camera_target;
static GroupPlayersGetPlayerGroupFunction group_players_get_player_group;
static GelPointerResolveEntityFunction gel_pointer_resolve_entity;
static TrackedEntityCleanupFunction tracked_entity_cleanup;
static CameraManagerGetCameraModeFunction camera_manager_get_camera_mode;
static CameraManagerSetCameraStateFunction camera_manager_set_camera_state;
static CameraInputEventFunction original_camera_input_event;
static GroupPlayersInCombatFunction group_players_in_combat;
static void *position_set_forward_function __attribute__((used));
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
static BOOL minimap_scheduled_owner_valid;
static BOOL minimap_scheduled_player_two;
static BOOL minimap_update_owner_valid;
static BOOL minimap_update_player_two;
static void *minimap_update_character;
static BOOL minimap_source_failure_this_frame;
static BOOL genuine_quick_menu_active_this_frame;
static BOOL quick_menu_live_player_two_available_this_frame;
static BOOL quick_menu_render_submit_isolation_logged;
static BOOL quick_menu_isolation_active;
static BOOL quick_menu_isolation_tail_active;
static BOOL quick_menu_isolation_failed_for_open_menu;
static BOOL quick_menu_genuine_visible_previous_frame;
static BOOL quick_menu_expected_player_two;
static BOOL quick_menu_render_phase_confirmed_this_frame;
static BOOL quick_menu_submit_seen_since_frame_end;
static BOOL quick_menu_owner_trace_valid;
static uint32_t quick_menu_owner_trace_values[5];
static BOOL quick_menu_owner_session_valid;
static void *quick_menu_owner_character;
static BOOL quick_menu_owner_player_two;
static BOOL quick_menu_owner_session_logged;
static SudekiMpQuickMenuSession quick_menu_session;
static uint32_t quick_menu_next_serial;
static SudekiMpPlayerTwoCollisionSelfCull
    player_two_collision_self_cull;
static BOOL player_two_collision_self_cull_logged;
static BOOL player_two_native_camera_input_logged;
static BOOL readable_memory(const void *pointer, size_t size);
static BOOL writable_memory(void *pointer, size_t size);
static void invalidate_dual_frame_cache(void);
static unsigned int refresh_shared_interaction_modal(void);
static void request_native_camera_manual_recreation(const char *reason);

static unsigned int quick_menu_find_character_reference(
    const void *object,
    unsigned int size,
    const void *character
) {
    unsigned int offset;

    if (object == NULL || character == NULL || size < sizeof(void *) ||
        !readable_memory(object, size)) {
        return 0xffffffffu;
    }
    for (offset = 0u; offset + sizeof(void *) <= size; offset += 4u) {
        if (*(const void **)((const uint8_t *)object + offset) == character) {
            return offset;
        }
    }
    return 0xffffffffu;
}

static BOOL quick_menu_find_character_graph_reference(
    const void *object,
    unsigned int size,
    const void *character,
    unsigned int depth,
    unsigned int *first_offset,
    unsigned int *second_offset
) {
    unsigned int offset;
    unsigned int direct;
    const void *child;

    if (first_offset == NULL || second_offset == NULL) {
        return FALSE;
    }
    direct = quick_menu_find_character_reference(object, size, character);
    if (direct != 0xffffffffu) {
        *first_offset = direct;
        *second_offset = 0xffffffffu;
        return TRUE;
    }
    if (depth == 0u || object == NULL || !readable_memory(object, size)) {
        return FALSE;
    }
    for (offset = 0u; offset + sizeof(void *) <= size; offset += 4u) {
        child = *(const void **)((const uint8_t *)object + offset);
        if (child == NULL || child == object ||
            !readable_memory(child, 0x500u)) {
            continue;
        }
        direct = quick_menu_find_character_reference(child, 0x500u, character);
        if (direct != 0xffffffffu) {
            *first_offset = offset;
            *second_offset = direct;
            return TRUE;
        }
        if (depth > 1u && quick_menu_find_character_graph_reference(
                child, 0x500u, character, depth - 1u,
                first_offset, second_offset)) {
            *first_offset = offset;
            return TRUE;
        }
    }
    return FALSE;
}
static BOOL render_only_swap_active;
static void **render_only_camera_slot;
static void *render_only_applied_state;
static void *render_only_original_state;
static BOOL skill_camera_routing_enabled;
static void *pending_skill_camera_caster;
static void *player_skill_cameras[2];
static void *player_skill_render_states[2];
static unsigned int skill_camera_request_sequence;
static unsigned int skill_speed_request_sequence;
static BOOL skill_time_scale_override_active;
static unsigned int skill_camera_trace_sequence;
static unsigned int skill_camera_trace_frame;
static DWORD skill_camera_trace_started_tick;
static BOOL skill_camera_trace_active;
static unsigned int skill_camera_history_logged_callbacks;
static DWORD skill_camera_history_tail_until;
static unsigned int second_player_camera_last_rejection;
static BOOL second_player_controller_camera_enabled;
static BOOL native_second_player_camera_collision_enabled;
static BOOL native_second_player_camera_collision_session_enabled;
static BOOL player_two_native_camera_recreate_requested;
static BOOL split_screen_ranged_model_isolation_enabled;
static BOOL spirit_strike_viewport_effect_isolation_enabled;
static MotionBlurPostRenderFunction original_motion_blur_post_render;
static ScreenshotPostRenderFunction original_screenshot_post_render;
static NativeHistoryResourceFactoryFunction native_history_resource_factory;
static void *spirit_player_two_history_wrapper;
static void *spirit_player_two_history_surface;
static UINT spirit_player_two_history_width;
static UINT spirit_player_two_history_height;
static BOOL spirit_history_resource_logged;
static BOOL spirit_effect_isolation_logged;
static BOOL spirit_capture_completion_logged;
static float second_player_controller_camera_deadzone;
static float second_player_controller_camera_yaw_speed;
static float second_player_controller_camera_pitch_speed;
static float second_player_controller_camera_maximum_pitch;
static BOOL player_two_camera_transform_initialized;
static unsigned int player_two_temporary_camera_policy =
    SUDEKIMP_TEMP_CAMERA_OUTSIDE;
static float player_two_camera_last_target[3];
static float player_two_camera_pitch_offset;
static BOOL player_two_first_person_camera_active;
static BOOL player_two_first_person_camera_requested;
static void *player_two_perspective_character;
static uint32_t player_two_perspective_actor_generation;
static BOOL player_two_forced_third_person_active;
static int player_two_camera_phase_last = -1;
static int player_two_rebased_phase = -1;
static float player_two_third_person_matrix[16];
static DWORD player_two_camera_input_last_tick;
static BOOL player_two_camera_input_logged;
static BOOL player_two_native_camera_bound;
static BOOL player_two_native_camera_attempted;
static BOOL player_two_native_camera_ready;
static BOOL player_two_native_camera_ready_logged;
static BOOL player_two_native_camera_fallback_logged;
static uint16_t player_two_native_camera_start_generation;
static void *player_two_native_camera_target_slot;
static void *player_two_native_camera_target_actor;
static uint32_t player_two_native_camera_actor_generation;
static void *player_two_native_camera_position_target;
static void *player_two_native_camera_look_at_target;
static unsigned int player_two_native_camera_stage;
static unsigned int player_two_native_camera_target_frame;
static unsigned int player_two_native_camera_state_frame;
static BOOL player_two_native_camera_recovery_armed;
static unsigned int player_two_native_camera_recovery_eligible_frames;
static unsigned int player_two_native_camera_recovery_last_frame;
/* GetPlayerGroupByPosition registers its wrapper with the engine PtrObj
 * registry. The isolated acceptance may request exactly one; the wrapper is
 * never cached, dereferenced again, or explicitly destroyed by the mod. */
static BOOL player_two_native_camera_wrapper_getter_attempted;
static unsigned int split_render_frame_sequence;
static BOOL ranged_model_isolation_logged;
static BOOL ranged_first_person_model_isolation_logged;
static BOOL ranged_model_animation_mirror_logged;
static BOOL ranged_first_person_animation_mirror_logged;
static BOOL ranged_player_two_first_person_rejection_logged;
static BOOL ranged_weapon_stage_logged;
static BOOL ranged_weapon_restore_logged;
static BOOL ranged_weapon_reattach_failure_logged;
static void *ranged_weapon_reattach_function __attribute__((used));
static void *render_locator_index_function __attribute__((used));
static void *ranged_locator_trace_character;
static int ranged_locator_trace_pitch_bucket;
static DWORD ranged_locator_trace_last_tick;
static unsigned int ranged_locator_trace_sequence;
static BOOL ranged_state_details_inventory_complete;
static BOOL ranged_state_details_inventory_pending_logged;
static DWORD ranged_transform_trace_last_tick;
static unsigned int ranged_transform_trace_sequence;
static void *ranged_translation_last_component;
static BOOL ranged_translation_trace_valid[RANGED_ANIMATION_CHANNEL_COUNT];
static unsigned int ranged_translation_last_animation_id[
    RANGED_ANIMATION_CHANNEL_COUNT
];
static unsigned int ranged_translation_last_bank;
static uint32_t ranged_translation_last_first_handle[
    RANGED_ANIMATION_CHANNEL_COUNT
];
static uint32_t ranged_translation_last_second_handle[
    RANGED_ANIMATION_CHANNEL_COUNT
];
static int ranged_translation_last_target_lookup[
    RANGED_ANIMATION_CHANNEL_COUNT
];
static int ranged_translation_last_source_lookup[
    RANGED_ANIMATION_CHANNEL_COUNT
];
static int ranged_translation_last_selector[
    RANGED_ANIMATION_CHANNEL_COUNT
];
static SudekiMpRangedModelRenderSwap ranged_model_render_swaps[
    RANGED_MODEL_RENDER_SWAP_COUNT
];
static SudekiMpRangedWorldCompositorState ranged_world_compositor_state;
static SudekiMpAnimationTraceSnapshot ranged_animation_trace[2];
static unsigned int ranged_animation_trace_sequence[2];
static unsigned int ranged_animation_progress_frame[2];
static void *ranged_animation_capture_failure_character[2];
static BOOL player_two_facing_logged;
static BOOL fixed_three_facing_logged[FIXED_THREE_SEAT_COUNT];
static int quick_menu_gate_last_state = -1;
static int spirit_presentation_last_state = -1;
static unsigned int spirit_presentation_logged_views;
static DWORD spirit_player_two_presentation_last_trace_tick;
static BOOL spirit_player_two_presentation_position_valid;
static float spirit_player_two_presentation_position[3];
static BOOL spirit_player_two_melee_locomotion_owned;
static BOOL spirit_player_two_melee_locomotion_moving;
static void *spirit_player_two_melee_locomotion_character;
static void *spirit_player_two_melee_locomotion_wrapper;
static void *spirit_player_two_melee_locomotion_renderer;
static BOOL runtime_split_enabled = TRUE;
static SudekiMpSplitScreenRuntimeAuthorizationQuery
    runtime_authorization_query;
static int runtime_authorization_last_state = -1;
static BOOL runtime_authorized_at_render_start;
static BOOL coop_role_lock_active;
static void *coop_locked_player_one;
static void *coop_locked_player_two;
static void *coop_locked_player_three;
static BOOL coop_roster_valid;
static unsigned int coop_roster_player_one_type;
static unsigned int coop_roster_player_two_type;
static SudekiMpCoopRosterAssignmentStore coop_roster_assignment_store;
static SudekiMpFixedThreeSeatRuntime fixed_three_runtime;
static BOOL coop_roster_participation_requested = TRUE;
static BOOL coop_roster_party_transition_active;
static BOOL coop_roster_runtime_release_pending;
static unsigned int coop_roster_rotation_attempts;
static BOOL coop_roster_rotation_previous;
static unsigned int coop_roster_last_presence_mask;
static BOOL coop_roster_ready_window_active;
static DWORD coop_roster_ready_since;
static void *coop_roster_ready_group;
static void *coop_roster_ready_controller_target;
static void *coop_roster_ready_party_front;
static void *coop_roster_ready_player_one;
static void *coop_roster_ready_player_two;
static void *coop_roster_ready_player_three;
static unsigned int coop_roster_ready_player_one_slot;
static unsigned int coop_roster_ready_player_two_slot;
static unsigned int coop_roster_ready_player_three_slot;
static int coop_roster_ready_party_count;
static int coop_roster_control_phase = -1;
static SudekiMpSplitScreenOverlayRenderer overlay_renderer;
static SudekiMpModOwnedBlacksmithActiveQuery
    mod_owned_blacksmith_active_query;

static BOOL coop_roster_native_transition_settled(
    unsigned int controller_mode_80,
    unsigned int controller_mode_84,
    unsigned int group_state_d0,
    unsigned int group_switching_d6,
    unsigned int group_state_d7,
    unsigned int next_character_action,
    unsigned int previous_character_action
);
static void reset_coop_roster_ready_window(void);
static BOOL fixed_three_assignment_selected(void);
static BOOL fixed_three_base_leases_exact(void);
static void fixed_three_invalidate_frame_cache(void);
static BOOL fixed_three_restore_render_swap(void);
static BOOL fixed_three_release_cameras(const char *reason);
static void fixed_three_release_frame_surfaces(void);
static BOOL release_player_two_camera(const char *reason);
static BOOL release_coop_roster_runtime(const char *reason);
static BOOL fixed_three_seat_view_exact(
    unsigned int seat_index,
    const void *character
);
static BOOL fixed_three_render_start_dispatch(BOOL runtime_authorized);
static void fixed_three_frame_end_dispatch(void);

static const char second_player_camera_name[] = "SudekiMP_P2";
static const char third_player_camera_name[] = "SudekiMP_P3";
static const char exploration_camera_state_name[] = "Exploration";
static const char spirit_history_resource_name[] = "_RenderTarget";
static const char *const ranged_observer_locator_names[] = {
    "WeaponFollow",
    "Staff1",
    "waist",
    "backbone_A",
    "backbone_B",
    "backbone_c",
    "backbone_Shoulders",
    "Left_Clavicle",
    "Right_Clavicle",
    "Left_Bound_UpperArm",
    "Left_Bound_LowerArm",
    "Left_Bound_Wrist",
    "Right_Bound_UpperArm",
    "Right_Bound_LowerArm",
    "Right_Bound_Wrist",
    "WeaponParent"
};
static const float player_two_first_person_eye_height = 1.55f;
/* Native DEFAULT profile baselines: exploration 3.5, combat 6.0.  The
 * observer cannot borrow the owner's first-person matrix, so preserve the
 * authored phase framing instead of using one fixed close-up distance. */
static const float player_two_exploration_camera_distance = 3.5f;
static const float player_two_combat_camera_distance = 6.0f;
static const float player_two_exploration_camera_height = 1.15f;
static const float player_two_combat_camera_height = 1.35f;
/* The native Exploration eye normally remains 3.5 units from its target.
 * This threshold only applies after its collision solver has pushed Camera 2
 * into Ailish's near body. */
static const float player_two_collision_self_cull_radius = 1.75f;
/* The supported image's native Exploration bootstrap at 0x004cf831 prepares
 * (0.0f, FALSE, 0x47c34ff3) for the internal transition installer called at
 * 0x004cf847.  CCameraManager::SetCameraState reaches that same installer
 * after resolving the state name; CCamera+0x40 is ExplorationStateData and
 * must not be interpreted as this tuple. */
static const uint32_t native_exploration_transition_b_bits =
    UINT32_C(0x47c34ff3);
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
static const uint8_t expected_camera_manager_set_render_camera_entry[] = {
    0x55, 0x8b, 0xec, 0x83, 0xe4, 0xf8
};
static const uint8_t expected_camera_manager_set_camera_target_entry[] = {
    0x55, 0x8b, 0xec, 0x83, 0xe4, 0xf0, 0x83, 0xec,
    0x74, 0x53, 0x8b, 0xd9
};
static const uint8_t expected_camera_manager_get_camera_mode_entry[] = {
    0x53, 0x8b, 0x5c, 0x24, 0x08, 0x55, 0x56, 0x57,
    0x8b, 0xe9, 0x85, 0xdb
};
static const uint8_t expected_camera_manager_set_camera_state_entry[] = {
    0xd9, 0x44, 0x24, 0x14, 0x8b, 0x44, 0x24, 0x10,
    0x56, 0x51, 0xd9, 0x1c, 0x24
};
static const uint8_t expected_camera_input_event_entry[] = {
    0x51, 0x56, 0x8b, 0xf1, 0x8b, 0x4e, 0x38, 0x33,
    0xc0, 0x85, 0xc9, 0x74
};
static const uint8_t expected_game_speed_set_mode_entry[] = {
    0x8b, 0x44, 0x24, 0x04, 0x89, 0x41, 0x24
};
static const uint8_t expected_fixed_alternate_speed[] = {
    0x29, 0x5c, 0x8f, 0x3d
};
static const uint8_t expected_position_set_forward_entry[] = {
    0x55, 0x8b, 0xec, 0x83, 0xe4, 0xf0, 0x83, 0xec,
    0x60, 0xd9, 0xee, 0xd9, 0x54, 0x24, 0x14
};
static const uint8_t expected_group_players_in_combat_entry[] = {
    0x8a, 0x81, 0xd4, 0x00, 0x00, 0x00, 0xc3
};
static const uint8_t expected_group_players_get_player_group_entry[] = {
    0x83, 0xec, 0x0c, 0x8b, 0x44, 0x24, 0x10, 0x8d,
    0x44, 0x40, 0x24, 0x8d, 0x0c, 0x81, 0x8d, 0x04,
    0x24, 0xe8, 0xca, 0xce, 0xfd, 0xff, 0x8b, 0x04,
    0x24, 0x85, 0xc0, 0x74, 0x1b, 0x56, 0x50, 0xe8,
    0x2c, 0xd5, 0xfd, 0xff, 0x8d, 0x4c, 0x24, 0x04,
    0x8b, 0xf0, 0xe8, 0xe1, 0xce, 0xfd, 0xff, 0x8b,
    0xc6, 0x5e, 0x83, 0xc4, 0x0c, 0xc2, 0x04, 0x00
};
static const uint8_t expected_gel_pointer_resolve_entity_prefix[] = {
    0x6a, 0xff, 0x68
};
static const uint8_t expected_gel_pointer_resolve_entity_suffix[] = {
    0x64, 0xa1, 0x00, 0x00, 0x00, 0x00, 0x50, 0x64,
    0x89, 0x25, 0x00, 0x00, 0x00, 0x00, 0x83, 0xec,
    0x0c, 0x53, 0x56, 0x57, 0x33, 0xff
};
static const uint8_t expected_tracked_entity_cleanup_entry[] = {
    0x8b, 0x01, 0x33, 0xd2, 0x3b, 0xc2, 0x74, 0x2d,
    0x56, 0x39, 0x48, 0x04
};
static const uint8_t expected_gel_group_ptr_deleting_destructor_entry[] = {
    0x56, 0x8b, 0xf1, 0x56, 0xe8, 0x17, 0x00, 0x00,
    0x00, 0xf6, 0x44, 0x24, 0x08, 0x01
};
static const uint8_t expected_group_players_character_switch_entry[] = {
    0x55, 0x8b, 0xec, 0x83, 0xe4, 0xf8, 0x83, 0xec,
    0x18, 0x83, 0xbe, 0xcc, 0x00, 0x00, 0x00, 0x01
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
static const uint8_t expected_history_resource_factory_entry[] = {
    0x55, 0x8b, 0xec, 0x83, 0xe4, 0xf8, 0x83, 0xec,
    0x0c, 0x53, 0x56, 0x57, 0x8b, 0xf9, 0x85, 0xc0
};
static const uint8_t expected_quick_menu_render_submit_entry[] = {
    0x56, 0x8b, 0xf1, 0x83, 0x7e, 0x38, 0x00, 0x75,
    0x2e, 0x80, 0xbe, 0xfe, 0x00, 0x00, 0x00, 0x00
};
static const uint8_t expected_quick_menu_input_entry[] = {
    0x8b, 0x44, 0x24, 0x04, 0x55, 0x56, 0x57, 0x8b,
    0xe9, 0x83, 0xf8, 0x19
};
static const uint8_t expected_quick_menu_close_prefix[] = {
    0x56, 0x8b, 0x35
};
static const uint8_t expected_quick_menu_close_suffix[] = {
    0x85, 0xf6, 0x74, 0x22, 0x80, 0x7e, 0x29, 0x00
};
static const uint8_t expected_quick_menu_start_suffix[] = {
    0x8b, 0x88, 0x74, 0x01, 0x00, 0x00, 0x8b, 0x11,
    0x8b, 0x42, 0x2c, 0x6a, 0x00, 0x6a, 0x00, 0x6a,
    0x10, 0xff, 0xd0, 0xc3
};
static const uint8_t expected_quick_menu_native_toggle_entry[] = {
    0x80, 0xb8, 0x8c, 0x00, 0x00, 0x00, 0x00, 0x74,
    0x46
};
static const uint8_t expected_hud_party_pointer_copy_entry[] = {
    0x8b, 0x11, 0x8b, 0xca, 0x89, 0x10, 0xc7, 0x40,
    0x04, 0x00, 0x00, 0x00, 0x00, 0xc7, 0x40, 0x08
};
static const uint8_t expected_shop_is_active_tail[] = {
    0x85, 0xc0, 0x74, 0x1e, 0x83, 0xb8, 0xb8, 0x00,
    0x00, 0x00, 0x07, 0x74, 0x0f, 0x8b, 0x40, 0x74
};
static const uint8_t expected_blacksmith_is_active_tail[] = {
    0x85, 0xc0, 0x74, 0x04, 0x8a, 0x40, 0x29, 0xc3,
    0x32, 0xc0, 0xc3
};
static const uint8_t expected_ingame_ui_controller_update_entry[] = {
    0x51, 0x56, 0x8b, 0xf1, 0x83, 0x7e, 0x2c, 0x00
};
static const uint8_t expected_ingame_ui_controller_render_entry[] = {
    0x56, 0x8b, 0xf1, 0x8b, 0x86, 0xb8, 0x00, 0x00,
    0x00, 0x48, 0x83, 0xf8, 0x11
};
static const uint8_t expected_ingame_ui_controller_input_entry[] = {
    0x83, 0xec, 0x18, 0x53, 0x55, 0x56, 0x8b, 0x74,
    0x24, 0x28, 0x8d, 0x46, 0xfb
};
static const uint8_t expected_ranged_weapon_reattach_entry[] = {
    0x53, 0x55, 0x8b, 0x6c, 0x24, 0x0c,
    0x56, 0x57, 0x85, 0xc0, 0x74, 0x6a
};
static const uint8_t expected_render_locator_index_entry[] = {
    0x56, 0x8b, 0x70, 0x14, 0x85, 0xf6, 0x74, 0x19,
    0x8b, 0x44, 0x24, 0x08, 0x57, 0x8b, 0x3e, 0xe8,
    0x3c, 0xd7, 0x11, 0x00, 0x8b, 0x57, 0x28, 0x50,
    0x8b, 0xce, 0xff, 0xd2, 0x5f, 0x5e, 0xc2, 0x04,
    0x00
};

static void **current_scene_render_camera_slot(void);
static BOOL character_has_resource_type(
    void *character_pointer,
    unsigned int expected_type
);

static BOOL relative_call_targets_rva(
    const uint8_t *base,
    uint32_t call_rva,
    uint32_t target_rva
) {
    int32_t displacement;

    if (base == NULL || base[call_rva] != 0xe8u) {
        return FALSE;
    }
    memcpy(&displacement, base + call_rva + 1u, sizeof(displacement));
    return base + call_rva + 5u + displacement == base + target_rva;
}

static BOOL quick_menu_owner_signatures_match(uint8_t *base) {
    uint32_t relocated_address;
    unsigned int index;

    if (base == NULL ||
        memcmp(
            base + RVA_QUICK_MENU_INPUT,
            expected_quick_menu_input_entry,
            sizeof(expected_quick_menu_input_entry)) != 0 ||
        memcmp(
            base + RVA_QUICK_MENU_CLOSE,
            expected_quick_menu_close_prefix,
            sizeof(expected_quick_menu_close_prefix)) != 0 ||
        memcmp(
            base + RVA_QUICK_MENU_CLOSE + 7u,
            expected_quick_menu_close_suffix,
            sizeof(expected_quick_menu_close_suffix)) != 0 ||
        base[RVA_QUICK_MENU_START] != 0xa1u ||
        memcmp(
            base + RVA_QUICK_MENU_START + 5u,
            expected_quick_menu_start_suffix,
            sizeof(expected_quick_menu_start_suffix)) != 0 ||
        memcmp(
            base + RVA_QUICK_MENU_NATIVE_TOGGLE,
            expected_quick_menu_native_toggle_entry,
            sizeof(expected_quick_menu_native_toggle_entry)) != 0 ||
        memcmp(
            base + RVA_HUD_PARTY_POINTER_COPY,
            expected_hud_party_pointer_copy_entry,
            sizeof(expected_hud_party_pointer_copy_entry)) != 0 ||
        memcmp(
            base + RVA_TRACKED_ENTITY_CLEANUP,
            expected_tracked_entity_cleanup_entry,
            sizeof(expected_tracked_entity_cleanup_entry)) != 0 ||
        *(void **)(base + RVA_QUICK_MENU_INPUT_VTABLE_SLOT) !=
            base + RVA_QUICK_MENU_INPUT ||
        !relative_call_targets_rva(
            base,
            RVA_QUICK_MENU_NATIVE_TOGGLE_CALL,
            RVA_QUICK_MENU_NATIVE_TOGGLE) ||
        !relative_call_targets_rva(
            base,
            RVA_QUICK_MENU_OWNER_COPY_DEFAULT_RECIPIENT,
            RVA_HUD_PARTY_POINTER_COPY)) {
        return FALSE;
    }
    memcpy(
        &relocated_address,
        base + RVA_QUICK_MENU_CLOSE +
            sizeof(expected_quick_menu_close_prefix),
        sizeof(relocated_address));
    if (relocated_address !=
            (uint32_t)(uintptr_t)(base + RVA_QUICK_MENU_GLOBAL)) {
        return FALSE;
    }
    memcpy(
        &relocated_address,
        base + RVA_QUICK_MENU_START + 1u,
        sizeof(relocated_address));
    if (relocated_address !=
            (uint32_t)(uintptr_t)(base + RVA_WORLD_SCENE_GLOBAL)) {
        return FALSE;
    }
    for (index = 0u; index < QUICK_MENU_OWNER_COPY_HOOK_COUNT; ++index) {
        if (!relative_call_targets_rva(
                base,
                quick_menu_owner_copy_call_rvas[index],
                RVA_HUD_PARTY_POINTER_COPY)) {
            return FALSE;
        }
    }
    return TRUE;
}

static BOOL quick_menu_is_active_signature_matches(uint8_t *base) {
    uint32_t relocated_singleton_address;

    if (base == NULL || base[RVA_QUICK_MENU_IS_ACTIVE] != 0xa1u ||
        memcmp(
            base + RVA_QUICK_MENU_IS_ACTIVE + 5u,
            "\x85\xc0\x74\x18",
            4u) != 0) {
        return FALSE;
    }
    memcpy(
        &relocated_singleton_address,
        base + RVA_QUICK_MENU_IS_ACTIVE + 1u,
        sizeof(relocated_singleton_address)
    );
    return relocated_singleton_address ==
        (uint32_t)(uintptr_t)(base + RVA_QUICK_MENU_GLOBAL);
}

static BOOL shared_interaction_modal_signatures_match(uint8_t *base) {
    uint32_t shop_singleton_address;
    uint32_t blacksmith_singleton_address;
    uint8_t *controller_vtable;
    uint8_t *shop_vtable;
    uint8_t *blacksmith_vtable;

    if (base == NULL || base[RVA_SHOP_IS_ACTIVE] != 0xa1u ||
        base[RVA_BLACKSMITH_IS_ACTIVE] != 0xa1u ||
        memcmp(
            base + RVA_SHOP_IS_ACTIVE + 5u,
            expected_shop_is_active_tail,
            sizeof(expected_shop_is_active_tail)) != 0 ||
        memcmp(
            base + RVA_BLACKSMITH_IS_ACTIVE + 5u,
            expected_blacksmith_is_active_tail,
            sizeof(expected_blacksmith_is_active_tail)) != 0 ||
        memcmp(
            base + RVA_INGAME_UI_CONTROLLER_UPDATE,
            expected_ingame_ui_controller_update_entry,
            sizeof(expected_ingame_ui_controller_update_entry)) != 0 ||
        memcmp(
            base + RVA_INGAME_UI_CONTROLLER_RENDER,
            expected_ingame_ui_controller_render_entry,
            sizeof(expected_ingame_ui_controller_render_entry)) != 0 ||
        memcmp(
            base + RVA_INGAME_UI_CONTROLLER_INPUT,
            expected_ingame_ui_controller_input_entry,
            sizeof(expected_ingame_ui_controller_input_entry)) != 0) {
        return FALSE;
    }
    memcpy(
        &shop_singleton_address,
        base + RVA_SHOP_IS_ACTIVE + 1u,
        sizeof(shop_singleton_address)
    );
    memcpy(
        &blacksmith_singleton_address,
        base + RVA_BLACKSMITH_IS_ACTIVE + 1u,
        sizeof(blacksmith_singleton_address)
    );
    if (shop_singleton_address != (uint32_t)(uintptr_t)(
            base + RVA_INGAME_UI_CONTROLLER_GLOBAL) ||
        blacksmith_singleton_address != (uint32_t)(uintptr_t)(
            base + RVA_BLACKSMITH_LAYER_GLOBAL)) {
        return FALSE;
    }
    controller_vtable = base + RVA_INGAME_UI_CONTROLLER_VTABLE;
    shop_vtable = base + RVA_SHOP_LAYER_VTABLE;
    blacksmith_vtable = base + RVA_BLACKSMITH_LAYER_VTABLE;
    return *(void **)(controller_vtable + 0x08u) ==
            base + RVA_INGAME_UI_CONTROLLER_UPDATE &&
        *(void **)(controller_vtable + 0x0cu) ==
            base + RVA_INGAME_UI_CONTROLLER_RENDER &&
        *(void **)(controller_vtable + 0x2cu) ==
            base + RVA_INGAME_UI_CONTROLLER_INPUT &&
        *(void **)(shop_vtable + 0x08u) ==
            base + RVA_SHOP_LAYER_UPDATE &&
        *(void **)(shop_vtable + 0x0cu) ==
            base + RVA_SHOP_LAYER_RENDER &&
        *(void **)(shop_vtable + 0x2cu) ==
            base + RVA_SHOP_LAYER_INPUT &&
        *(void **)(shop_vtable + 0x48u) ==
            base + RVA_SHOP_LAYER_RESOURCE_CREATE &&
        *(void **)(shop_vtable + 0x4cu) ==
            base + RVA_SHOP_LAYER_RESOURCE_DESTROY &&
        *(void **)(blacksmith_vtable + 0x08u) ==
            base + RVA_BLACKSMITH_LAYER_UPDATE &&
        *(void **)(blacksmith_vtable + 0x0cu) ==
            base + RVA_BLACKSMITH_LAYER_RENDER &&
        *(void **)(blacksmith_vtable + 0x2cu) ==
            base + RVA_BLACKSMITH_LAYER_INPUT &&
        *(void **)(blacksmith_vtable + 0x48u) ==
            base + RVA_BLACKSMITH_LAYER_RESOURCE_CREATE &&
        *(void **)(blacksmith_vtable + 0x4cu) ==
            base + RVA_BLACKSMITH_LAYER_RESOURCE_DESTROY;
}

static BOOL animation_renderer_signatures_match(uint8_t *base) {
    uint8_t *vtable;

    if (base == NULL) {
        return FALSE;
    }
    vtable = base + RVA_ANIMATION_RENDERER_VTABLE;
    return *(void **)(vtable + 0x40u) ==
            base + RVA_ANIMATION_RENDERER_LOOKUP &&
        *(void **)(vtable + 0xf8u) ==
            base + RVA_ANIMATION_RENDERER_COUNT &&
        *(void **)(vtable + 0xfcu) ==
            base + RVA_ANIMATION_RENDERER_SELECTOR_SET &&
        *(void **)(vtable + 0x100u) ==
            base + RVA_ANIMATION_RENDERER_SELECTOR_GET &&
        *(void **)(vtable + 0x104u) ==
            base + RVA_ANIMATION_RENDERER_RATE_SET &&
        *(void **)(vtable + 0x108u) ==
            base + RVA_ANIMATION_RENDERER_RATE_GET &&
        *(void **)(vtable + 0x10cu) ==
            base + RVA_ANIMATION_RENDERER_TIME_SET &&
        *(void **)(vtable + 0x110u) ==
            base + RVA_ANIMATION_RENDERER_TIME_GET &&
        *(void **)(vtable + 0x114u) ==
            base + RVA_ANIMATION_RENDERER_STATE_SET &&
        *(void **)(vtable + 0x118u) ==
            base + RVA_ANIMATION_RENDERER_STATE_GET &&
        *(void **)(vtable + 0x144u) ==
            base + RVA_ANIMATION_RENDERER_BLEND_SET &&
        *(void **)(vtable + 0x148u) ==
            base + RVA_ANIMATION_RENDERER_BLEND_GET;
}

/* SkillTraceImageTest maps the supported PE inertly rather than registering it
 * with the Windows loader. Resolve its export table directly when
 * GetProcAddress is therefore unavailable; the same exact name/RVA check is
 * applied afterward in both live and inert images. */
static FARPROC resolve_mapped_pe_export(
    HMODULE game_module,
    const char *name
) {
    uint8_t *base = (uint8_t *)game_module;
    IMAGE_DOS_HEADER *dos;
    IMAGE_NT_HEADERS32 *nt;
    IMAGE_DATA_DIRECTORY *directory;
    IMAGE_EXPORT_DIRECTORY *exports;
    DWORD *function_rvas;
    DWORD *name_rvas;
    WORD *ordinals;
    DWORD image_size;
    size_t expected_length;
    DWORD index;

    if (base == NULL || name == NULL ||
        !readable_memory(base, sizeof(IMAGE_DOS_HEADER))) {
        return NULL;
    }
    dos = (IMAGE_DOS_HEADER *)base;
    if (dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew <= 0 ||
        !readable_memory(
            base + dos->e_lfanew, sizeof(IMAGE_NT_HEADERS32))) {
        return NULL;
    }
    nt = (IMAGE_NT_HEADERS32 *)(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE ||
        nt->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR32_MAGIC) {
        return NULL;
    }
    image_size = nt->OptionalHeader.SizeOfImage;
    directory = &nt->OptionalHeader.DataDirectory[
        IMAGE_DIRECTORY_ENTRY_EXPORT];
    if (directory->VirtualAddress == 0u ||
        directory->VirtualAddress > image_size -
            sizeof(IMAGE_EXPORT_DIRECTORY)) {
        return NULL;
    }
    exports = (IMAGE_EXPORT_DIRECTORY *)(
        base + directory->VirtualAddress);
    if (exports->NumberOfFunctions == 0u || exports->NumberOfNames == 0u ||
        exports->NumberOfFunctions > image_size / sizeof(DWORD) ||
        exports->NumberOfNames > image_size / sizeof(DWORD) ||
        exports->AddressOfFunctions > image_size -
            exports->NumberOfFunctions * sizeof(DWORD) ||
        exports->AddressOfNames > image_size -
            exports->NumberOfNames * sizeof(DWORD) ||
        exports->AddressOfNameOrdinals > image_size -
            exports->NumberOfNames * sizeof(WORD)) {
        return NULL;
    }
    function_rvas = (DWORD *)(base + exports->AddressOfFunctions);
    name_rvas = (DWORD *)(base + exports->AddressOfNames);
    ordinals = (WORD *)(base + exports->AddressOfNameOrdinals);
    expected_length = strlen(name);
    for (index = 0u; index < exports->NumberOfNames; ++index) {
        DWORD name_rva = name_rvas[index];
        WORD ordinal = ordinals[index];
        DWORD function_rva;

        if (name_rva >= image_size || expected_length >=
                image_size - name_rva ||
            memcmp(base + name_rva, name, expected_length) != 0 ||
            base[name_rva + expected_length] != '\0') {
            continue;
        }
        if (ordinal >= exports->NumberOfFunctions) {
            return NULL;
        }
        function_rva = function_rvas[ordinal];
        if (function_rva == 0u || function_rva >= image_size ||
            (function_rva >= directory->VirtualAddress &&
             function_rva < directory->VirtualAddress + directory->Size)) {
            return NULL;
        }
        return (FARPROC)(base + function_rva);
    }
    return NULL;
}

static BOOL native_camera_wrapper_signatures_match(
    HMODULE game_module,
    uint8_t *base,
    void **resolved_get_player_group
) {
    static const char export_name[] =
        "?GetPlayerGroupByPosition@CGroupPlayers@@QAEPAVGELPointer@@H@Z";
    FARPROC exported_getter;
    uint32_t relocated_resolver_handler;
    uint8_t *vtable;

    if (game_module == NULL || base == NULL) {
        return FALSE;
    }
    exported_getter = GetProcAddress(game_module, export_name);
    if (exported_getter == NULL) {
        exported_getter = resolve_mapped_pe_export(
            game_module, export_name);
    }
    if (exported_getter == NULL ||
        (uint8_t *)(uintptr_t)exported_getter !=
            base + RVA_GROUP_PLAYERS_GET_PLAYER_GROUP ||
        memcmp(
            base + RVA_GROUP_PLAYERS_GET_PLAYER_GROUP,
            expected_group_players_get_player_group_entry,
            sizeof(expected_group_players_get_player_group_entry)) != 0 ||
        memcmp(
            base + RVA_GEL_POINTER_RESOLVE_ENTITY,
            expected_gel_pointer_resolve_entity_prefix,
            sizeof(expected_gel_pointer_resolve_entity_prefix)) != 0 ||
        memcmp(
            base + RVA_GEL_POINTER_RESOLVE_ENTITY + 7u,
            expected_gel_pointer_resolve_entity_suffix,
            sizeof(expected_gel_pointer_resolve_entity_suffix)) != 0 ||
        memcmp(
            base + RVA_TRACKED_ENTITY_CLEANUP,
            expected_tracked_entity_cleanup_entry,
            sizeof(expected_tracked_entity_cleanup_entry)) != 0 ||
        memcmp(
            base + RVA_GEL_GROUP_PTR_DELETING_DESTRUCTOR,
            expected_gel_group_ptr_deleting_destructor_entry,
            sizeof(expected_gel_group_ptr_deleting_destructor_entry)) != 0) {
        return FALSE;
    }
    memcpy(
        &relocated_resolver_handler,
        base + RVA_GEL_POINTER_RESOLVE_ENTITY + 3u,
        sizeof(relocated_resolver_handler)
    );
    vtable = base + RVA_GEL_GROUP_PTR_VTABLE;
    if (relocated_resolver_handler != (uint32_t)(uintptr_t)(
            base + RVA_GEL_POINTER_RESOLVER_HANDLER) ||
        *(void **)(vtable + 0x00u) !=
            base + RVA_GEL_GROUP_PTR_DELETING_DESTRUCTOR ||
        *(void **)(vtable + 0x10u) !=
            base + RVA_GEL_GROUP_PTR_GET_RAW_ENTITY ||
        *(void **)(vtable + 0x2cu) !=
            base + RVA_GEL_GROUP_PTR_TYPE_NAME) {
        return FALSE;
    }
    if (resolved_get_player_group != NULL) {
        *resolved_get_player_group = (void *)(uintptr_t)exported_getter;
    }
    return TRUE;
}

static BOOL character_switch_consumer_signatures_match(uint8_t *base) {
    return base != NULL &&
        memcmp(
            base + RVA_GROUP_PLAYERS_PREVIOUS_CHARACTER,
            expected_group_players_character_switch_entry,
            sizeof(expected_group_players_character_switch_entry)) == 0 &&
        memcmp(
            base + RVA_GROUP_PLAYERS_NEXT_CHARACTER,
            expected_group_players_character_switch_entry,
            sizeof(expected_group_players_character_switch_entry)) == 0;
}

static uint32_t float_bits(float value) {
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

/* Every CCamera is registered with the same native input dispatcher.  The
 * game broadcasts Player 1 camera events to every registered camera, so the
 * named Player 2 camera must not consume those events.  Update scheduling is
 * a separate UpdateNode registration and remains active. */
static void SUDEKIMP_THISCALL route_camera_input_event(
    void *camera,
    const void *event
) {
    if (camera == player_two_camera ||
        (fixed_three_runtime.cameras_acquired &&
         (camera == fixed_three_runtime.cameras[1] ||
          camera == fixed_three_runtime.cameras[2]))) {
        return;
    }
    if (original_camera_input_event != NULL) {
        original_camera_input_event(camera, event);
    }
}

static void reset_player_two_controller_camera(void) {
    if (player_two_first_person_camera_active &&
        writable_memory(player_two_render_state, 0xdcu)) {
        unsigned int index;
        BOOL saved_matrix_valid = TRUE;

        for (index = 0u; index < 16u; ++index) {
            if (!isfinite(player_two_third_person_matrix[index])) {
                saved_matrix_valid = FALSE;
                break;
            }
        }
        if (fabsf(player_two_third_person_matrix[15] - 1.0f) > 0.001f) {
            saved_matrix_valid = FALSE;
        }
        if (saved_matrix_valid) {
            uint16_t *generation = (uint16_t *)(
                (uint8_t *)player_two_render_state + 0x2cu);

            memcpy(
                (uint8_t *)player_two_render_state + 0x90u,
                player_two_third_person_matrix,
                sizeof(player_two_third_person_matrix));
            ++*generation;
        }
    }
    player_two_camera_transform_initialized = FALSE;
    player_two_temporary_camera_policy = SUDEKIMP_TEMP_CAMERA_OUTSIDE;
    ZeroMemory(
        player_two_camera_last_target,
        sizeof(player_two_camera_last_target)
    );
    player_two_camera_pitch_offset = 0.0f;
    player_two_first_person_camera_active = FALSE;
    player_two_first_person_camera_requested = FALSE;
    player_two_perspective_character = NULL;
    player_two_perspective_actor_generation = 0u;
    player_two_forced_third_person_active = FALSE;
    player_two_camera_phase_last = -1;
    player_two_rebased_phase = -1;
    ZeroMemory(
        player_two_third_person_matrix,
        sizeof(player_two_third_person_matrix)
    );
    player_two_camera_input_last_tick = 0u;
    player_two_camera_input_logged = FALSE;
    player_two_native_camera_bound = FALSE;
    player_two_native_camera_attempted = FALSE;
    player_two_native_camera_ready = FALSE;
    player_two_native_camera_ready_logged = FALSE;
    player_two_native_camera_input_logged = FALSE;
    player_two_native_camera_fallback_logged = FALSE;
    player_two_native_camera_start_generation = 0u;
    player_two_native_camera_target_slot = NULL;
    player_two_native_camera_target_actor = NULL;
    player_two_native_camera_actor_generation = 0u;
    player_two_native_camera_position_target = NULL;
    player_two_native_camera_look_at_target = NULL;
    player_two_native_camera_stage = SUDEKIMP_NATIVE_CAMERA_STAGE_IDLE;
    player_two_native_camera_target_frame = 0u;
    player_two_native_camera_state_frame = 0u;
    player_two_native_camera_recovery_armed = FALSE;
    player_two_native_camera_recovery_eligible_frames = 0u;
    player_two_native_camera_recovery_last_frame = 0u;
}

static BOOL settled_temporary_zone_active(void) {
    uint8_t *world;
    uint8_t *descriptor;

    if (game_base == NULL || !readable_memory(
            game_base + RVA_WORLD_GLOBAL, sizeof(world))) {
        return FALSE;
    }
    world = *(uint8_t **)(game_base + RVA_WORLD_GLOBAL);
    if (!readable_memory(world, 0x39bu)) {
        return FALSE;
    }
    descriptor = *(uint8_t **)(world + 0x0cu);
    return descriptor != NULL &&
        readable_memory(descriptor, 0x38u) &&
        *(uint32_t *)(descriptor + 0x34u) == 4u &&
        *(void **)(world + 0x14u) == NULL &&
        *(uint8_t *)(world + 0x399u) != 0u &&
        *(uint8_t *)(world + 0x39au) != 0u;
}

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

static BOOL writable_memory(void *pointer, size_t size) {
    MEMORY_BASIC_INFORMATION information;
    uintptr_t start;
    uintptr_t end;
    uintptr_t region_end;
    DWORD protection;

    if (pointer == NULL || size == 0u ||
        VirtualQuery(pointer, &information, sizeof(information)) == 0 ||
        information.State != MEM_COMMIT ||
        (information.Protect & (PAGE_NOACCESS | PAGE_GUARD)) != 0) {
        return FALSE;
    }
    protection = information.Protect & 0xffu;
    if (protection != PAGE_READWRITE &&
        protection != PAGE_WRITECOPY &&
        protection != PAGE_EXECUTE_READWRITE &&
        protection != PAGE_EXECUTE_WRITECOPY) {
        return FALSE;
    }
    start = (uintptr_t)pointer;
    end = start + size;
    region_end = (uintptr_t)information.BaseAddress +
        information.RegionSize;
    return end >= start && end <= region_end;
}

static BOOL executable_memory(const void *pointer) {
    MEMORY_BASIC_INFORMATION information;
    DWORD protection;

    if (pointer == NULL ||
        VirtualQuery(pointer, &information, sizeof(information)) == 0 ||
        information.State != MEM_COMMIT ||
        (information.Protect & (PAGE_NOACCESS | PAGE_GUARD)) != 0) {
        return FALSE;
    }
    protection = information.Protect & 0xffu;
    return protection == PAGE_EXECUTE ||
        protection == PAGE_EXECUTE_READ ||
        protection == PAGE_EXECUTE_READWRITE ||
        protection == PAGE_EXECUTE_WRITECOPY;
}

/*
 * RVA 0x000D8280 uses Sudeki's internal register convention:
 * EAX is the active locator-name text, while the weapon pointer is the one
 * callee-cleaned stack argument.  The helper is deliberately narrower than
 * replaying the full ranged presentation transition; native model switching
 * calls this exact function after changing CPosition+0xB4.
 */
__attribute__((naked, noinline, used))
static unsigned char call_ranged_weapon_reattach(
    void *weapon __attribute__((unused)),
    const char *locator_name __attribute__((unused)),
    void *function __attribute__((unused))
) {
    __asm__ volatile(
        "movl 8(%esp), %eax\n\t"
        "movl 12(%esp), %edx\n\t"
        "pushl 4(%esp)\n\t"
        "call *%edx\n\t"
        "ret\n\t"
    );
}

static const char *ranged_weapon_primary_locator_name(void *weapon_pointer) {
    uint8_t *weapon = (uint8_t *)weapon_pointer;
    const char *locator_name;
    unsigned int length;

    if (!readable_memory(weapon, 0x278u)) {
        return NULL;
    }
    locator_name = (*(uint32_t *)(weapon + 0x270u) & 0x80000000u) != 0u ?
        (const char *)(weapon + 0x274u) :
        *(const char **)(weapon + 0x274u);
    if (locator_name == NULL) {
        return NULL;
    }
    for (length = 0u; length < 64u; ++length) {
        if (!readable_memory(locator_name + length, 1u)) {
            return NULL;
        }
        if (locator_name[length] == '\0') {
            return length == 0u ? NULL : locator_name;
        }
    }
    return NULL;
}

static BOOL reattach_ranged_weapon_to_current_model(
    void *weapon_pointer,
    const char *phase
) {
    uint8_t *weapon = (uint8_t *)weapon_pointer;
    const char *locator_name;
    int primary_locator_before;
    int secondary_locator_before;
    int primary_locator_after;
    int secondary_locator_after;
    unsigned char result;
    BOOL *logged;

    if (!readable_memory(weapon, 0x208u) ||
        *(void **)(weapon + 0xf4u) == NULL) {
        return TRUE;
    }
    locator_name = ranged_weapon_primary_locator_name(weapon);
    if (locator_name == NULL ||
        !executable_memory(ranged_weapon_reattach_function)) {
        if (!ranged_weapon_reattach_failure_logged) {
            ranged_weapon_reattach_failure_logged = TRUE;
            SudekiMpLogFormat(
                "split_screen_render event=ranged_weapon_reattach phase=%s result=rejected reason=%s weapon=0x%08lx policy=native_narrow_helper_or_no_write\r\n",
                phase,
                locator_name == NULL ?
                    "primary_locator_name_unavailable" :
                    "native_helper_unavailable",
                (unsigned long)(uintptr_t)weapon
            );
        }
        return FALSE;
    }

    primary_locator_before = *(int *)(weapon + 0xecu);
    secondary_locator_before = *(int *)(weapon + 0x1fcu);
    result = call_ranged_weapon_reattach(
        weapon,
        locator_name,
        ranged_weapon_reattach_function
    );
    primary_locator_after = *(int *)(weapon + 0xecu);
    secondary_locator_after = *(int *)(weapon + 0x1fcu);
    if (result == 0u) {
        if (!ranged_weapon_reattach_failure_logged) {
            ranged_weapon_reattach_failure_logged = TRUE;
            SudekiMpLogFormat(
                "split_screen_render event=ranged_weapon_reattach phase=%s result=rejected reason=native_helper_returned_false weapon=0x%08lx locator_name=%s primary_locator_before=%d primary_locator_after=%d secondary_locator_before=%d secondary_locator_after=%d policy=native_narrow_helper_or_no_write\r\n",
                phase,
                (unsigned long)(uintptr_t)weapon,
                locator_name,
                primary_locator_before,
                primary_locator_after,
                secondary_locator_before,
                secondary_locator_after
            );
        }
        return FALSE;
    }

    logged = strcmp(phase, "observer_world_stage") == 0 ?
        &ranged_weapon_stage_logged : &ranged_weapon_restore_logged;
    if (!*logged) {
        *logged = TRUE;
        SudekiMpLogFormat(
            "split_screen_render event=ranged_weapon_reattach phase=%s result=applied weapon=0x%08lx locator_name=%s primary_parent=0x%08lx primary_wrapper=0x%08lx primary_locator_before=%d primary_locator_after=%d secondary_parent=0x%08lx secondary_wrapper=0x%08lx secondary_locator_before=%d secondary_locator_after=%d policy=native_model_switch_helper_recalculate_locator_and_local_matrix\r\n",
            phase,
            (unsigned long)(uintptr_t)weapon,
            locator_name,
            (unsigned long)(uintptr_t)*(void **)(weapon + 0xd4u),
            (unsigned long)(uintptr_t)*(void **)(weapon + 0xf4u),
            primary_locator_before,
            primary_locator_after,
            (unsigned long)(uintptr_t)*(void **)(weapon + 0x1e4u),
            (unsigned long)(uintptr_t)*(void **)(weapon + 0x204u),
            secondary_locator_before,
            secondary_locator_after
        );
    }
    return TRUE;
}

static BOOL ranged_transform_trace_matrix(
    const void *base,
    size_t offset,
    const float **matrix
) {
    const uint8_t *bytes = (const uint8_t *)base;

    if (matrix == NULL ||
        !readable_memory(bytes, offset + sizeof(float) * 16u)) {
        return FALSE;
    }
    *matrix = (const float *)(bytes + offset);
    return TRUE;
}

static void log_ranged_transform_trace(
    const char *phase,
    const SudekiMpRangedModelRenderSwap *swap
) {
    const float *player_one_matrix;
    const float *player_two_matrix;
    const float *position_matrix;
    const float *position_source_matrix;
    const float *first_person_matrix;
    const float *world_matrix;
    const float *weapon_matrix;
    uint8_t *arbiter = NULL;
    uint8_t *controller = NULL;
    uint32_t arbiter_flags = 0u;
    unsigned int controller_first_person = 0u;
    int weapon_matches_pre = -1;

    if (swap == NULL || !swap->transform_trace_sample ||
        phase == NULL ||
        !ranged_transform_trace_matrix(
            player_one_render_state, 0x90u, &player_one_matrix) ||
        !ranged_transform_trace_matrix(
            player_two_render_state, 0x90u, &player_two_matrix) ||
        !ranged_transform_trace_matrix(
            swap->position, 0x30u, &position_matrix) ||
        !ranged_transform_trace_matrix(
            swap->position, 0xc0u, &position_source_matrix) ||
        !ranged_transform_trace_matrix(
            swap->first_person_render_object,
            0x90u,
            &first_person_matrix) ||
        !ranged_transform_trace_matrix(
            swap->world_render_object, 0x90u, &world_matrix) ||
        !ranged_transform_trace_matrix(
            swap->weapon, 0x70u, &weapon_matrix)) {
        return;
    }
    if (readable_memory(swap->character, 0x94u)) {
        arbiter = *(uint8_t **)((uint8_t *)swap->character + 0x90u);
    }
    if (readable_memory(arbiter, 0x54u)) {
        arbiter_flags = *(uint32_t *)(arbiter + 0x50u);
    }
    if (game_base != NULL && readable_memory(
            game_base + RVA_CHARACTER_CONTROLLER_GLOBAL,
            sizeof(controller))) {
        controller = *(uint8_t **)(
            game_base + RVA_CHARACTER_CONTROLLER_GLOBAL
        );
    }
    if (readable_memory(controller, 0xa1u)) {
        controller_first_person = controller[0xa0u] & 1u;
    }
    if (swap->saved_weapon_local_matrix_valid) {
        weapon_matches_pre = memcmp(
            weapon_matrix,
            swap->saved_weapon_local_matrix,
            sizeof(swap->saved_weapon_local_matrix)
        ) == 0 ? 1 : 0;
    }
    SudekiMpLogFormat(
        "split_screen_render event=ranged_transform_trace sequence=%u phase=%s character=0x%08lx attachment=0x%08lx arbiter_flags=0x%08lx controller_first_person=%u p1_forward=%.5f,%.5f,%.5f p1_up=%.5f,%.5f,%.5f p1_eye=%.5f,%.5f,%.5f p2_forward=%.5f,%.5f,%.5f position_forward=%.5f,%.5f,%.5f position_source_forward=%.5f,%.5f,%.5f first_person_forward=%.5f,%.5f,%.5f world_forward=%.5f,%.5f,%.5f weapon_forward=%.5f,%.5f,%.5f weapon_translation=%.5f,%.5f,%.5f weapon_matches_pre=%d policy=read_only_three_phase_pitch_and_restore_diagnostic\r\n",
        swap->transform_trace_sequence,
        phase,
        (unsigned long)(uintptr_t)swap->character,
        (unsigned long)(uintptr_t)(
            readable_memory(swap->position, 0xb8u) ?
                *(void **)((uint8_t *)swap->position + 0xb4u) : NULL
        ),
        (unsigned long)arbiter_flags,
        controller_first_person,
        player_one_matrix[8], player_one_matrix[9], player_one_matrix[10],
        player_one_matrix[4], player_one_matrix[5], player_one_matrix[6],
        player_one_matrix[12], player_one_matrix[13], player_one_matrix[14],
        player_two_matrix[8], player_two_matrix[9], player_two_matrix[10],
        position_matrix[8], position_matrix[9], position_matrix[10],
        position_source_matrix[8], position_source_matrix[9],
            position_source_matrix[10],
        first_person_matrix[8], first_person_matrix[9],
            first_person_matrix[10],
        world_matrix[8], world_matrix[9], world_matrix[10],
        weapon_matrix[8], weapon_matrix[9], weapon_matrix[10],
        weapon_matrix[12], weapon_matrix[13], weapon_matrix[14],
        weapon_matches_pre
    );
}

/*
 * RVA 0x000C5FF0 uses Sudeki's internal register convention: EAX is the
 * render object and the locator-name C string is the one callee-cleaned stack
 * argument. The native helper hashes the exact case-sensitive name and calls
 * provider vtable +0x28. Keeping that path intact avoids guessing Sudeki's
 * ResourceName hash while this pass remains observation-only.
 */
__attribute__((naked, noinline, used))
static int call_render_locator_index(
    void *render_object __attribute__((unused)),
    const char *locator_name __attribute__((unused)),
    void *function __attribute__((unused))
) {
    __asm__ volatile(
        "movl 4(%esp), %eax\n\t"
        "movl 12(%esp), %edx\n\t"
        "pushl 8(%esp)\n\t"
        "call *%edx\n\t"
        "ret\n\t"
    );
}

static BOOL ranged_observer_locator_matrix(
    void *render_object_pointer,
    const char *locator_name,
    int *locator_index_result,
    const float **matrix_result,
    const char **reason_result
) {
    uint8_t *render_object = (uint8_t *)render_object_pointer;
    uint8_t *provider;
    void **vtable;
    RenderLocatorMatrixFunction get_matrix;
    const float *matrix;
    int locator_index;
    unsigned int value_index;

    if (locator_index_result != NULL) {
        *locator_index_result = -1;
    }
    if (matrix_result != NULL) {
        *matrix_result = NULL;
    }
    if (reason_result != NULL) {
        *reason_result = "unknown";
    }
    if (!readable_memory(render_object, 0x18u)) {
        if (reason_result != NULL) {
            *reason_result = "render_object_unreadable";
        }
        return FALSE;
    }
    if (locator_name == NULL ||
        !executable_memory(render_locator_index_function)) {
        if (reason_result != NULL) {
            *reason_result = "native_index_helper_unavailable";
        }
        return FALSE;
    }
    provider = *(uint8_t **)(render_object + 0x14u);
    if (!readable_memory(provider, sizeof(void *))) {
        if (reason_result != NULL) {
            *reason_result = "provider_unreadable";
        }
        return FALSE;
    }
    vtable = *(void ***)provider;
    if (!readable_memory(vtable, 0x2cu) ||
        !executable_memory(vtable[0x28u / sizeof(void *)])) {
        if (reason_result != NULL) {
            *reason_result = "provider_index_method_unavailable";
        }
        return FALSE;
    }
    locator_index = call_render_locator_index(
        render_object,
        locator_name,
        render_locator_index_function
    );
    if (locator_index_result != NULL) {
        *locator_index_result = locator_index;
    }
    if (locator_index < 0) {
        if (reason_result != NULL) {
            *reason_result = "name_not_present";
        }
        return FALSE;
    }
    if (!executable_memory(vtable[0x24u / sizeof(void *)])) {
        if (reason_result != NULL) {
            *reason_result = "provider_matrix_method_unavailable";
        }
        return FALSE;
    }
    get_matrix = (RenderLocatorMatrixFunction)(
        vtable[0x24u / sizeof(void *)]
    );
    matrix = get_matrix(provider, locator_index);
    if (!readable_memory(matrix, sizeof(float) * 16u)) {
        if (reason_result != NULL) {
            *reason_result = "locator_matrix_unreadable";
        }
        return FALSE;
    }
    for (value_index = 0u; value_index < 16u; ++value_index) {
        if (!isfinite(matrix[value_index])) {
            if (reason_result != NULL) {
                *reason_result = "locator_matrix_non_finite";
            }
            return FALSE;
        }
    }
    if (matrix_result != NULL) {
        *matrix_result = matrix;
    }
    if (reason_result != NULL) {
        *reason_result = "resolved";
    }
    return TRUE;
}

static const char *ranged_locator_pitch_bucket_name(int pitch_bucket) {
    switch (pitch_bucket) {
    case 0: return "neutral";
    case 1: return "up";
    case 2: return "down";
    default: return "transition";
    }
}

static void __attribute__((unused)) log_ranged_observer_locator_inventory(
    const SudekiMpRangedModelRenderSwap *swap
) {
    const float *first_person_matrix;
    const float *locator_matrix;
    const char *reason;
    uint32_t component_bits[6];
    float component_values[6];
    float horizontal_length;
    float pitch;
    DWORD now;
    int pitch_bucket;
    int locator_index;
    unsigned int name_index;
    unsigned int sequence;

    if (swap == NULL ||
        !character_has_resource_type(swap->character, 0x01u) ||
        !readable_memory(swap->component, 0x160u) ||
        !readable_memory(swap->first_person_render_object, 0xd0u) ||
        !readable_memory(swap->world_render_object, 0x18u)) {
        return;
    }
    first_person_matrix = (const float *)(
        (const uint8_t *)swap->first_person_render_object + 0x90u
    );
    horizontal_length = sqrtf(
        first_person_matrix[8] * first_person_matrix[8] +
        first_person_matrix[10] * first_person_matrix[10]
    );
    pitch = atan2f(first_person_matrix[9], horizontal_length);
    if (!isfinite(horizontal_length) || !isfinite(pitch)) {
        return;
    }
    if (fabsf(pitch) <= 0.04f) {
        pitch_bucket = 0;
    } else if (pitch >= 0.15f) {
        pitch_bucket = 1;
    } else if (pitch <= -0.15f) {
        pitch_bucket = 2;
    } else {
        return;
    }
    if (ranged_locator_trace_character != swap->character) {
        ranged_locator_trace_character = swap->character;
        ranged_locator_trace_pitch_bucket = -1;
        ranged_locator_trace_last_tick = 0u;
    }
    if (ranged_locator_trace_pitch_bucket == pitch_bucket) {
        return;
    }
    now = GetTickCount();
    if (ranged_locator_trace_last_tick != 0u &&
        (DWORD)(now - ranged_locator_trace_last_tick) < 250u) {
        return;
    }
    ranged_locator_trace_pitch_bucket = pitch_bucket;
    ranged_locator_trace_last_tick = now;
    sequence = ++ranged_locator_trace_sequence;
    memcpy(
        component_bits,
        (const uint8_t *)swap->component + 0x148u,
        sizeof(component_bits)
    );
    memcpy(component_values, component_bits, sizeof(component_values));
    SudekiMpLogFormat(
        "split_screen_render event=ranged_observer_locator_inventory sequence=%u phase=sample character=0x%08lx component=0x%08lx render_object=0x%08lx provider=0x%08lx pitch_bucket=%s pitch=%.5f component_148=0x%08lx/%.5f component_14c=0x%08lx/%.5f component_150=0x%08lx/%.5f component_154=0x%08lx/%.5f component_158=0x%08lx/%.5f component_15c=0x%08lx/%.5f policy=read_only_exact_name_inventory_no_locator_or_bone_write\r\n",
        sequence,
        (unsigned long)(uintptr_t)swap->character,
        (unsigned long)(uintptr_t)swap->component,
        (unsigned long)(uintptr_t)swap->world_render_object,
        (unsigned long)(uintptr_t)*(void **)(
            (uint8_t *)swap->world_render_object + 0x14u),
        ranged_locator_pitch_bucket_name(pitch_bucket),
        pitch,
        (unsigned long)component_bits[0], component_values[0],
        (unsigned long)component_bits[1], component_values[1],
        (unsigned long)component_bits[2], component_values[2],
        (unsigned long)component_bits[3], component_values[3],
        (unsigned long)component_bits[4], component_values[4],
        (unsigned long)component_bits[5], component_values[5]
    );
    for (name_index = 0u;
         name_index < sizeof(ranged_observer_locator_names) /
             sizeof(ranged_observer_locator_names[0]);
         ++name_index) {
        locator_matrix = NULL;
        locator_index = -1;
        reason = "unknown";
        if (ranged_observer_locator_matrix(
                swap->world_render_object,
                ranged_observer_locator_names[name_index],
                &locator_index,
                &locator_matrix,
                &reason)) {
            SudekiMpLogFormat(
                "split_screen_render event=ranged_observer_locator_inventory sequence=%u phase=locator result=resolved pitch_bucket=%s name=%s index=%d matrix=0x%08lx right=%.5f,%.5f,%.5f up=%.5f,%.5f,%.5f forward=%.5f,%.5f,%.5f translation=%.5f,%.5f,%.5f policy=read_only_no_locator_or_bone_write\r\n",
                sequence,
                ranged_locator_pitch_bucket_name(pitch_bucket),
                ranged_observer_locator_names[name_index],
                locator_index,
                (unsigned long)(uintptr_t)locator_matrix,
                locator_matrix[0], locator_matrix[1], locator_matrix[2],
                locator_matrix[4], locator_matrix[5], locator_matrix[6],
                locator_matrix[8], locator_matrix[9], locator_matrix[10],
                locator_matrix[12], locator_matrix[13], locator_matrix[14]
            );
        } else {
            SudekiMpLogFormat(
                "split_screen_render event=ranged_observer_locator_inventory sequence=%u phase=locator result=unresolved pitch_bucket=%s name=%s index=%d reason=%s policy=read_only_no_locator_or_bone_write\r\n",
                sequence,
                ranged_locator_pitch_bucket_name(pitch_bucket),
                ranged_observer_locator_names[name_index],
                locator_index,
                reason
            );
        }
    }
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

static BOOL ranged_presentation_parts(
    void *character_pointer,
    uint8_t **position_result,
    uint8_t **component_result,
    void **first_person_wrapper_result,
    void **world_wrapper_result
) {
    uint8_t *character = (uint8_t *)character_pointer;
    uint8_t *position;
    uint8_t *component;
    void *first_person_wrapper;
    void *world_wrapper;

    if (!readable_memory(character, 0x138u)) {
        return FALSE;
    }
    position = *(uint8_t **)(character + 0x44u);
    component = *(uint8_t **)(character + 0x134u);
    if (!readable_memory(position, 0xb8u) ||
        !readable_memory(component, 0x168u)) {
        return FALSE;
    }
    first_person_wrapper = *(void **)(component + 0x160u);
    world_wrapper = *(void **)(component + 0x164u);
    if (first_person_wrapper == world_wrapper ||
        !readable_memory(first_person_wrapper, 0x14u) ||
        !readable_memory(world_wrapper, 0x14u)) {
        return FALSE;
    }
    if (position_result != NULL) {
        *position_result = position;
    }
    if (component_result != NULL) {
        *component_result = component;
    }
    if (first_person_wrapper_result != NULL) {
        *first_person_wrapper_result = first_person_wrapper;
    }
    if (world_wrapper_result != NULL) {
        *world_wrapper_result = world_wrapper;
    }
    return TRUE;
}

static BOOL character_has_resource_type(
    void *character_pointer,
    unsigned int expected_type
) {
    uint8_t *character = (uint8_t *)character_pointer;
    uint8_t *resource;
    void **vtable;
    CharacterResourceTypeFunction get_type;

    if (!readable_memory(
            character,
            CHARACTER_RESOURCE_OBJECT_OFFSET + sizeof(void *))) {
        return FALSE;
    }
    resource = character + CHARACTER_RESOURCE_OBJECT_OFFSET;
    vtable = *(void ***)resource;
    if (!readable_memory(vtable, 5u * sizeof(void *))) {
        return FALSE;
    }
    get_type = (CharacterResourceTypeFunction)vtable[4];
    if (!readable_memory((const void *)get_type, sizeof(void *))) {
        return FALSE;
    }
    return get_type(resource) == expected_type;
}

/* Ailish's ranged first-person bank has no world counterpart for these IDs.
 * The native world bank uses the ordinary missile combo family instead. */
static unsigned int ailish_world_animation_id(unsigned int animation_id) {
    switch (animation_id) {
    case 0x05u: return 0x02u; /* IDLE_STRAFE -> IDLE_NORMAL */
    case 0x8cu: return 0x85u; /* MISSILE_STRAFE_COMBO1 -> MISSILE_COMBO1 */
    case 0x8du: return 0x86u; /* MISSILE_STRAFE_COMBO2 -> MISSILE_COMBO2 */
    case 0x8eu: return 0x87u; /* MISSILE_STRAFE_COMBO3 -> MISSILE_COMBO3 */
    default: return 0u;
    }
}

static BOOL ailish_first_person_only_animation(unsigned int animation_id) {
    switch (animation_id) {
    case 0xc1u:
    case 0xc2u:
    case 0xc3u:
        return TRUE;
    default:
        return FALSE;
    }
}

static BOOL active_group_in_combat(void) {
    void *group;

    if (game_base == NULL || group_players_in_combat == NULL ||
        !readable_memory(
            game_base + RVA_ACTIVE_GROUP_GLOBAL,
            sizeof(group))) {
        return FALSE;
    }
    group = *(void **)(game_base + RVA_ACTIVE_GROUP_GLOBAL);
    return readable_memory(group, 0xd5u) &&
        group_players_in_combat(group) != 0u;
}

static BOOL character_is_using_skill(void *character) {
    uint8_t *arbiter;

    if (!readable_memory(character, 0x94u)) {
        return FALSE;
    }
    arbiter = *(uint8_t **)((uint8_t *)character + 0x90u);
    return readable_memory(arbiter, 0x54u) &&
        (*(uint32_t *)(arbiter + 0x50u) & 0x10u) != 0u;
}

static BOOL player_two_should_use_first_person(void) {
    BOOL requested = split_screen_ranged_model_isolation_enabled &&
        player_two_character != NULL && active_group_in_combat() &&
        ranged_presentation_parts(
            player_two_character,
            NULL,
            NULL,
            NULL,
            NULL
        );

    if (requested && !ranged_player_two_first_person_rejection_logged) {
        ranged_player_two_first_person_rejection_logged = TRUE;
        SudekiMpLogWrite(
            "split_screen_render event=player_two_first_person_camera phase=rejected reason=raw_first_person_wrapper_attachment_crashes_native_model_renderer fallback=preserve_third_person_camera_and_world_wrapper policy=observer_side_world_model_isolation_remains_enabled\r\n"
        );
    }
    return FALSE;
}

static BOOL player_two_should_use_camera_first_person(void) {
    SudekiMpPlayerStatehood *statehood;
    const SudekiMpPlayerLease *lease;

    if (!player_two_first_person_camera_requested) {
        return FALSE;
    }
    statehood = SudekiMpPlayerStatehoodRuntime();
    lease = statehood == NULL ? NULL : &statehood->players[1];
    if (player_two_perspective_character == NULL || lease == NULL ||
        !lease->human_present || lease->actor == 0u ||
        lease->actor_generation == 0u ||
        lease->actor != (uintptr_t)player_two_perspective_character ||
        lease->actor_generation != player_two_perspective_actor_generation) {
        player_two_first_person_camera_requested = FALSE;
        player_two_perspective_character = NULL;
        player_two_perspective_actor_generation = 0u;
        return FALSE;
    }
    return SudekiMpSplitScreenPlayerTwoPerspectiveAvailable(
        player_two_perspective_character);
}

static void log_player_two_camera_phase(
    BOOL player_one_first_person,
    BOOL combat_phase,
    BOOL skill_phase
) {
    int phase = player_one_first_person ?
        (skill_phase ? 3 : (combat_phase ? 2 : 1)) : 0;
    const char *name = phase == 3 ? "skill" :
        (phase == 2 ? "combat" :
         (phase == 1 ? "first_person_observer" : "third_person"));

    if (phase == player_two_camera_phase_last) {
        return;
    }
    player_two_camera_phase_last = phase;
    SudekiMpLogFormat(
        "split_screen_render event=player_two_camera phase=profile_transition profile=%s player_one_first_person=%u combat=%u skill_camera=%u distance_bits=0x%08lx height_bits=0x%08lx policy=native_default_profile_baseline_observer_only\r\n",
        name,
        player_one_first_person ? 1u : 0u,
        combat_phase ? 1u : 0u,
        skill_phase ? 1u : 0u,
        (unsigned long)float_bits(
            combat_phase ? player_two_combat_camera_distance :
                player_two_exploration_camera_distance
        ),
        (unsigned long)float_bits(
            combat_phase ? player_two_combat_camera_height :
                player_two_exploration_camera_height
        )
    );
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

static BOOL quick_menu_visible(void) {
    return quick_menu_is_active != NULL && quick_menu_is_active() != FALSE;
}

static void *quick_menu_singleton(void) {
    void *quick_menu;

    if (game_base == NULL || !readable_memory(
            game_base + RVA_QUICK_MENU_GLOBAL,
            sizeof(quick_menu))) {
        return NULL;
    }
    quick_menu = *(void **)(game_base + RVA_QUICK_MENU_GLOBAL);
    return readable_memory(quick_menu, 0x2au) &&
        *(void **)quick_menu == game_base + RVA_QUICK_MENU_VTABLE ?
            quick_menu : NULL;
}

static BOOL genuine_quick_menu_visible(void) {
    uint8_t *quick_menu = (uint8_t *)quick_menu_singleton();

    return quick_menu != NULL && quick_menu[0x29u] != 0u;
}

BOOL SudekiMpSplitScreenSharedInteractionModalShouldQuiesce(
    BOOL inspector_installed,
    unsigned int observation,
    BOOL recovery_pending
) {
    if (!inspector_installed) {
        return FALSE;
    }
    return observation != SUDEKIMP_SHARED_INTERACTION_MODAL_NONE ||
        recovery_pending;
}

BOOL SudekiMpSplitScreenViewportPortraitAssignmentNeeded(
    BOOL modal_construction_active,
    unsigned int current_portrait_enum,
    unsigned int desired_portrait_enum
) {
    return !modal_construction_active &&
        desired_portrait_enum < 16u &&
        current_portrait_enum != desired_portrait_enum;
}

BOOL SudekiMpSplitScreenSharedInteractionRecoveryEligible(
    BOOL runtime_enabled,
    BOOL participation_requested,
    BOOL roles_locked,
    BOOL camera_feature_enabled,
    BOOL cache_feature_enabled,
    BOOL camera_pair_ready,
    BOOL cache_pair_ready
) {
    return runtime_enabled && participation_requested && roles_locked &&
        camera_feature_enabled && cache_feature_enabled &&
        camera_pair_ready && cache_pair_ready;
}

BOOL SudekiMpSplitScreenSharedInteractionRecoveryPendingNext(
    BOOL recovery_pending,
    BOOL modal_closed_edge,
    BOOL modal_had_live_split,
    BOOL current_eligible,
    BOOL fresh_cache_pair_ready
) {
    if (!current_eligible || fresh_cache_pair_ready) {
        return FALSE;
    }
    if (recovery_pending) {
        return TRUE;
    }
    return modal_closed_edge && modal_had_live_split;
}

unsigned int SudekiMpSplitScreenClassifySharedInteractionModal(
    BOOL inspection_valid,
    BOOL shop_reported_active,
    BOOL blacksmith_reported_active,
    unsigned int current_ui_mode,
    unsigned int next_ui_mode
) {
    BOOL shop_active;
    BOOL blacksmith_active;

    if (!inspection_valid) {
        return SUDEKIMP_SHARED_INTERACTION_MODAL_UNCERTAIN;
    }
    shop_active = shop_reported_active || current_ui_mode == 7u ||
        current_ui_mode == 8u || current_ui_mode == 9u ||
        next_ui_mode == 7u || next_ui_mode == 8u ||
        next_ui_mode == 9u;
    blacksmith_active = blacksmith_reported_active ||
        current_ui_mode == 5u || current_ui_mode == 6u ||
        next_ui_mode == 5u || next_ui_mode == 6u;
    if (shop_active && blacksmith_active) {
        return SUDEKIMP_SHARED_INTERACTION_MODAL_UNCERTAIN;
    }
    if (shop_active) {
        return SUDEKIMP_SHARED_INTERACTION_MODAL_SHOP;
    }
    if (blacksmith_active) {
        return SUDEKIMP_SHARED_INTERACTION_MODAL_BLACKSMITH;
    }
    return SUDEKIMP_SHARED_INTERACTION_MODAL_NONE;
}

BOOL SudekiMpSplitScreenNativeBlacksmithReportedActive(
    BOOL reported_active,
    BOOL mod_owned_active,
    BOOL native_layer_active,
    unsigned int current_ui_mode,
    unsigned int next_ui_mode
) {
    /* UIBlackSmithActive is also a script wait predicate. A mod-owned session
     * deliberately keeps that export true without constructing/activating the
     * singleton. Exclude it only when all independent native state agrees;
     * any layer/mode disagreement preserves the conservative full-width path. */
    if (mod_owned_active && !native_layer_active &&
        current_ui_mode != 5u && current_ui_mode != 6u &&
        next_ui_mode != 5u && next_ui_mode != 6u) {
        return FALSE;
    }
    return reported_active || native_layer_active;
}

static BOOL shared_interaction_recovery_runtime_eligible(
    BOOL require_fresh_frames,
    const char **reason
) {
    void *manager = current_camera_manager();
    BOOL camera_pair_ready;
    BOOL cache_pair_ready;

    if (reason != NULL) {
        *reason = "eligible";
    }
    camera_pair_ready =
        manager != NULL && manager == second_player_camera_manager &&
        readable_memory(player_one_camera, 0x38u) &&
        readable_memory(player_two_camera, 0x38u) &&
        readable_memory(player_one_render_state, 0xdcu) &&
        readable_memory(player_two_render_state, 0xdcu) &&
        readable_memory(player_one_character, 0x94u) &&
        readable_memory(player_two_character, 0x94u);
    cache_pair_ready =
        readable_memory(frame_cache_device, sizeof(void *)) &&
        readable_memory(player_one_frame_surface, sizeof(void *)) &&
        readable_memory(player_two_frame_surface, sizeof(void *)) &&
        readable_memory(player_one_frame_texture, sizeof(void *)) &&
        readable_memory(player_two_frame_texture, sizeof(void *)) &&
        frame_cache_description.width != 0u &&
        frame_cache_description.height != 0u &&
        (!require_fresh_frames ||
         (player_one_frame_valid && player_two_frame_valid));
    if (!split_screen_render_installed && reason != NULL) {
        *reason = "inspector_not_installed";
    } else if (!runtime_split_enabled && reason != NULL) {
        *reason = "split_runtime_disabled";
    } else if ((!coop_roster_valid ||
                !coop_roster_participation_requested) && reason != NULL) {
        *reason = "player_two_participation_inactive";
    } else if (!coop_role_lock_active && reason != NULL) {
        *reason = "co_op_roles_unlocked";
    } else if ((!second_player_camera_enabled ||
                !dual_camera_frame_cache_enabled) && reason != NULL) {
        *reason = "camera_cache_features_disabled";
    } else if (!camera_pair_ready && reason != NULL) {
        *reason = "live_camera_pair_unavailable";
    } else if (!cache_pair_ready && reason != NULL) {
        *reason = require_fresh_frames ?
            "fresh_cache_pair_unavailable" :
            "cache_resources_unavailable";
    }
    return SudekiMpSplitScreenSharedInteractionRecoveryEligible(
        split_screen_render_installed && runtime_split_enabled,
        coop_roster_valid && coop_roster_participation_requested,
        coop_role_lock_active,
        second_player_camera_enabled,
        dual_camera_frame_cache_enabled,
        camera_pair_ready,
        cache_pair_ready
    );
}

static const char *shared_interaction_modal_name(unsigned int observation) {
    switch (observation) {
        case SUDEKIMP_SHARED_INTERACTION_MODAL_SHOP:
            return "shop";
        case SUDEKIMP_SHARED_INTERACTION_MODAL_BLACKSMITH:
            return "blacksmith";
        case SUDEKIMP_SHARED_INTERACTION_MODAL_UNCERTAIN:
            return "uncertain";
        case SUDEKIMP_SHARED_INTERACTION_MODAL_SAVE_BOOK:
            return "save_book";
        default:
            return "none";
    }
}

static unsigned int inspect_shared_interaction_modal(
    const char **reason
) {
    uint8_t *controller;
    uint8_t *shop;
    uint8_t *blacksmith;
    void *controller_shop;
    void *controller_blacksmith;
    unsigned int current_mode;
    unsigned int next_mode;
    BOOL shop_active;
    BOOL blacksmith_active;
    BOOL mod_owned_blacksmith_active;
    BOOL native_blacksmith_layer_active;

    if (reason != NULL) {
        *reason = "observed";
    }
    if (!split_screen_render_installed) {
        if (reason != NULL) {
            *reason = "inspector_not_installed";
        }
        return SUDEKIMP_SHARED_INTERACTION_MODAL_NONE;
    }
    if (InterlockedCompareExchange(
            &native_save_modal_opening,
            0,
            0) != 0) {
        if (reason != NULL) {
            *reason = "save_book_pre_native_lifecycle";
        }
        return SUDEKIMP_SHARED_INTERACTION_MODAL_SAVE_BOOK;
    }
    if (game_base == NULL || shop_is_active == NULL ||
        blacksmith_is_active == NULL) {
        if (reason != NULL) {
            *reason = "native_modal_bindings_unavailable";
        }
        return SUDEKIMP_SHARED_INTERACTION_MODAL_UNCERTAIN;
    }
    if (!readable_memory(
            game_base + RVA_INGAME_UI_CONTROLLER_GLOBAL,
            sizeof(controller)) ||
        !readable_memory(
            game_base + RVA_SHOP_LAYER_GLOBAL,
            sizeof(shop)) ||
        !readable_memory(
            game_base + RVA_BLACKSMITH_LAYER_GLOBAL,
            sizeof(blacksmith))) {
        if (reason != NULL) {
            *reason = "native_modal_singleton_slots_unreadable";
        }
        return SUDEKIMP_SHARED_INTERACTION_MODAL_UNCERTAIN;
    }
    controller = *(uint8_t **)(
        game_base + RVA_INGAME_UI_CONTROLLER_GLOBAL
    );
    shop = *(uint8_t **)(game_base + RVA_SHOP_LAYER_GLOBAL);
    blacksmith = *(uint8_t **)(
        game_base + RVA_BLACKSMITH_LAYER_GLOBAL
    );
    if (controller == NULL && shop == NULL && blacksmith == NULL) {
        return SUDEKIMP_SHARED_INTERACTION_MODAL_NONE;
    }
    if (!readable_memory(
            controller,
            INGAME_UI_CONTROLLER_NEXT_MODE_OFFSET + sizeof(unsigned int)) ||
        *(void **)controller !=
            game_base + RVA_INGAME_UI_CONTROLLER_VTABLE) {
        if (reason != NULL) {
            *reason = "native_ui_controller_identity_uncertain";
        }
        return SUDEKIMP_SHARED_INTERACTION_MODAL_UNCERTAIN;
    }
    controller_shop = *(void **)(
        controller + INGAME_UI_CONTROLLER_SHOP_OFFSET
    );
    controller_blacksmith = *(void **)(
        controller + INGAME_UI_CONTROLLER_BLACKSMITH_OFFSET
    );
    if (controller_shop != shop || controller_blacksmith != blacksmith ||
        !readable_memory(shop, SHOP_LAYER_STATE_OFFSET + sizeof(int)) ||
        *(void **)shop != game_base + RVA_SHOP_LAYER_VTABLE ||
        !readable_memory(
            blacksmith,
            BLACKSMITH_LAYER_STATE_OFFSET + sizeof(int)) ||
        *(void **)blacksmith != game_base + RVA_BLACKSMITH_LAYER_VTABLE) {
        if (reason != NULL) {
            *reason = "native_modal_layer_identity_uncertain";
        }
        return SUDEKIMP_SHARED_INTERACTION_MODAL_UNCERTAIN;
    }
    current_mode = *(unsigned int *)(
        controller + INGAME_UI_CONTROLLER_CURRENT_MODE_OFFSET
    );
    next_mode = *(unsigned int *)(
        controller + INGAME_UI_CONTROLLER_NEXT_MODE_OFFSET
    );
    shop_active = shop_is_active() != FALSE;
    mod_owned_blacksmith_active =
        mod_owned_blacksmith_active_query != NULL &&
        mod_owned_blacksmith_active_query() != FALSE;
    native_blacksmith_layer_active =
        blacksmith[BLACKSMITH_LAYER_ACTIVE_OFFSET] != 0u;
    /* Avoid invoking the hooked script predicate while the mod-owned
     * lifecycle is proven: its TRUE means "script still waiting", not
     * "native singleton visible". Any native disagreement fails closed. */
    blacksmith_active =
        SudekiMpSplitScreenNativeBlacksmithReportedActive(
            mod_owned_blacksmith_active ? FALSE :
                blacksmith_is_active() != FALSE,
            mod_owned_blacksmith_active,
            native_blacksmith_layer_active,
            current_mode,
            next_mode);
    return SudekiMpSplitScreenClassifySharedInteractionModal(
        TRUE,
        shop_active,
        blacksmith_active,
        current_mode,
        next_mode
    );
}

static unsigned int refresh_shared_interaction_modal(void) {
    const char *reason;
    const char *eligibility_reason;
    unsigned int previous_observation =
        shared_interaction_modal_observation;
    unsigned int observation = inspect_shared_interaction_modal(&reason);
    BOOL was_full_width = previous_observation !=
        SUDEKIMP_SHARED_INTERACTION_MODAL_NONE;
    BOOL full_width = observation !=
        SUDEKIMP_SHARED_INTERACTION_MODAL_NONE;
    BOOL recovery_was_pending =
        shared_interaction_modal_recovery_pending;
    BOOL current_recovery_eligible;
    SudekiMpInteractionKind observed_kind = SUDEKIMP_INTERACTION_NONE;

    if (!split_screen_render_installed) {
        shared_interaction_modal_observation =
            SUDEKIMP_SHARED_INTERACTION_MODAL_NONE;
        shared_interaction_modal_recovery_pending = FALSE;
        shared_interaction_modal_had_live_split = FALSE;
        return SUDEKIMP_SHARED_INTERACTION_MODAL_NONE;
    }
    shared_interaction_modal_observation = observation;
    if (was_full_width != full_width) {
        if (full_width) {
            if (recovery_was_pending) {
                /* A reopened modal inherits the cache refresh already owed by
                 * the previous close. Its intentionally partial cache cannot
                 * satisfy the ordinary fresh-pair entry test. */
                shared_interaction_modal_had_live_split =
                    shared_interaction_recovery_runtime_eligible(
                        FALSE,
                        &eligibility_reason
                    );
            } else {
                shared_interaction_modal_had_live_split =
                    shared_interaction_recovery_runtime_eligible(
                        TRUE,
                        &eligibility_reason
                    );
            }
        }
        invalidate_dual_frame_cache();
        if (full_width) {
            shared_interaction_modal_recovery_pending = FALSE;
            player_two_view_requested = FALSE;
            SudekiMpLogFormat(
                "split_screen_render event=shared_interaction_modal phase=enter kind=%s reason=%s owner=player_one presentation=native_full_width recovery_eligible=%u recovery_reason=%s cache_policy=invalidate_on_edge_bypass_dual_compositor\r\n",
                shared_interaction_modal_name(observation),
                reason,
                shared_interaction_modal_had_live_split ? 1u : 0u,
                eligibility_reason
            );
        } else {
            current_recovery_eligible =
                shared_interaction_recovery_runtime_eligible(
                    FALSE,
                    &eligibility_reason
                );
            shared_interaction_modal_recovery_pending =
                SudekiMpSplitScreenSharedInteractionRecoveryPendingNext(
                    FALSE,
                    TRUE,
                    shared_interaction_modal_had_live_split,
                    current_recovery_eligible,
                    FALSE
                );
            shared_interaction_modal_had_live_split = FALSE;
            SudekiMpLogFormat(
                "split_screen_render event=shared_interaction_modal phase=exit kind=%s owner=player_one presentation=%s recovery_eligible=%u recovery_reason=%s cache_policy=invalidate_on_edge_then_rebuild_both_views_or_fail_open\r\n",
                shared_interaction_modal_name(previous_observation),
                shared_interaction_modal_recovery_pending ?
                    "full_width_recovery_barrier" : "native",
                current_recovery_eligible ? 1u : 0u,
                eligibility_reason
            );
        }
    } else if (full_width && shared_interaction_modal_had_live_split &&
               !shared_interaction_recovery_runtime_eligible(
                    FALSE,
                    &eligibility_reason)) {
        shared_interaction_modal_had_live_split = FALSE;
        SudekiMpLogFormat(
            "split_screen_render event=shared_interaction_modal phase=recovery_disarmed reason=%s timing=modal_active policy=fail_open_after_close\r\n",
            eligibility_reason
        );
    } else if (!full_width && recovery_was_pending) {
        current_recovery_eligible =
            shared_interaction_recovery_runtime_eligible(
                FALSE,
                &eligibility_reason
            );
        shared_interaction_modal_recovery_pending =
            SudekiMpSplitScreenSharedInteractionRecoveryPendingNext(
                recovery_was_pending,
                FALSE,
                FALSE,
                current_recovery_eligible,
                FALSE
            );
        if (!shared_interaction_modal_recovery_pending) {
            SudekiMpLogFormat(
                "split_screen_render event=shared_interaction_modal phase=recovery_cancelled reason=%s policy=fail_open_do_not_rearm_on_rejoin\r\n",
                eligibility_reason
            );
        }
    }
    if (observation == SUDEKIMP_SHARED_INTERACTION_MODAL_SHOP) {
        observed_kind = SUDEKIMP_INTERACTION_SHOP;
    } else if (observation ==
            SUDEKIMP_SHARED_INTERACTION_MODAL_BLACKSMITH) {
        observed_kind = SUDEKIMP_INTERACTION_BLACKSMITH;
    }
    if (observed_kind != SUDEKIMP_INTERACTION_NONE) {
        SudekiMpPlayerStatehoodObserveNativeModal(
            SudekiMpPlayerStatehoodRuntime(),
            observed_kind,
            0u,
            GetTickCount()
        );
        shared_interaction_modal_published_kind = observed_kind;
    } else if (observation == SUDEKIMP_SHARED_INTERACTION_MODAL_NONE &&
               shared_interaction_modal_published_kind !=
                    SUDEKIMP_INTERACTION_NONE) {
        SudekiMpPlayerStatehoodObserveNativeModal(
            SudekiMpPlayerStatehoodRuntime(),
            SUDEKIMP_INTERACTION_NONE,
            0u,
            GetTickCount()
        );
        shared_interaction_modal_published_kind = SUDEKIMP_INTERACTION_NONE;
    }
    return observation;
}

BOOL SudekiMpSplitScreenNativeSaveModalOpening(void) {
    if (!split_screen_render_installed) {
        return FALSE;
    }
    InterlockedExchange(&native_save_modal_opening, 1);
    /* Enter synchronously: the caller is contractually still before the
     * native save continuation, so no cached split frame can survive into
     * construction and no later portrait refresh can race that continuation. */
    return refresh_shared_interaction_modal() ==
        SUDEKIMP_SHARED_INTERACTION_MODAL_SAVE_BOOK;
}

void SudekiMpSplitScreenNativeSaveModalClosed(void) {
    if (InterlockedExchange(&native_save_modal_opening, 0) == 0 ||
        !split_screen_render_installed) {
        return;
    }
    /* A stable inspector may close the edge immediately.  Any uncertain
     * native teardown remains full-width until inspection becomes exact. */
    refresh_shared_interaction_modal();
}

BOOL SudekiMpSplitScreenNativeSaveModalActive(void) {
    return split_screen_render_installed &&
        (InterlockedCompareExchange(
            &native_save_modal_opening,
            0,
            0) != 0 ||
         shared_interaction_modal_observation ==
            SUDEKIMP_SHARED_INTERACTION_MODAL_SAVE_BOOK);
}

BOOL SudekiMpSplitScreenNativeMovieOpening(void) {
    LONG depth;
    BOOL release_confirmed;

    if (!split_screen_render_installed) {
        /* The movie hook is also used by standalone traversal/title modes.
         * With no renderer installed this is an inert pass-through, not a
         * failed movie gate. */
        return TRUE;
    }
    depth = InterlockedIncrement(&native_movie_gate_depth);
    if (depth > 1) {
        return TRUE;
    }
    player_two_view_requested = FALSE;
    invalidate_dual_frame_cache();
    fixed_three_invalidate_frame_cache();
    if (fixed_three_runtime.transaction_active) {
        InterlockedDecrement(&native_movie_gate_depth);
        SetLastError(ERROR_BUSY);
        SudekiMpLogWrite(
            "split_screen_render event=native_movie_gate phase=reject "
            "reason=render_transaction_active policy=do_not_enter_movie\r\n");
        return FALSE;
    }
    if (coop_roster_valid) {
        /* FMA07 continues synchronously into native companion destruction
         * after MoviePlay returns.  Keep the roster selection, but release
         * every live runtime ownership in the proven order: named cameras,
         * role lock, P3 AI, then P2 AI.  The normal 250ms exact-ready window
         * is the post-movie quarantine before any later reacquire. */
        release_confirmed = release_coop_roster_runtime(
            "native_movie_opening");
    } else {
        BOOL fixed_released = fixed_three_release_cameras(
            "native_movie_opening");
        BOOL legacy_released = release_player_two_camera(
            "native_movie_opening");

        release_confirmed = fixed_released && legacy_released;
    }
    if (!release_confirmed) {
        InterlockedDecrement(&native_movie_gate_depth);
        SetLastError(ERROR_BUSY);
        SudekiMpLogWrite(
            "split_screen_render event=native_movie_gate phase=reject "
            "reason=runtime_release_unconfirmed "
            "policy=do_not_enter_destructive_movie_continuation\r\n");
        return FALSE;
    }
    SudekiMpLogWrite(
        "split_screen_render event=native_movie_gate phase=enter "
        "presentation=native_full_width runtime=released "
        "cache=invalidated input=quiesced\r\n");
    return TRUE;
}

void SudekiMpSplitScreenNativeMovieClosed(void) {
    LONG depth;

    depth = InterlockedCompareExchange(&native_movie_gate_depth, 0, 0);
    if (depth <= 0) {
        return;
    }
    depth = InterlockedDecrement(&native_movie_gate_depth);
    if (depth != 0) {
        return;
    }
    player_two_view_requested = FALSE;
    invalidate_dual_frame_cache();
    fixed_three_invalidate_frame_cache();
    SudekiMpLogWrite(
        "split_screen_render event=native_movie_gate phase=exit "
        "presentation=native_full_width cache=requires_all_fresh_frames "
        "reacquire=ordinary_exact_gameplay_gate\r\n");
}

BOOL SudekiMpSplitScreenNativeMovieActive(void) {
    return split_screen_render_installed &&
        InterlockedCompareExchange(&native_movie_gate_depth, 0, 0) > 0;
}

BOOL SudekiMpSplitScreenSharedInteractionModalActive(void) {
    unsigned int observation;

    if (!split_screen_render_installed) {
        return FALSE;
    }
    if (SudekiMpSplitScreenNativeMovieActive()) {
        return TRUE;
    }
    observation = refresh_shared_interaction_modal();
    return SudekiMpSplitScreenSharedInteractionModalShouldQuiesce(
        split_screen_render_installed,
        observation,
        shared_interaction_modal_recovery_pending
    );
}

static BOOL quick_menu_live_player_two_ready(void) {
    void *manager = current_camera_manager();

    return dual_camera_frame_cache_enabled &&
        second_player_camera_enabled &&
        manager != NULL &&
        manager == second_player_camera_manager &&
        readable_memory(manager, 0x4cu) &&
        player_one_camera != NULL &&
        player_two_camera != NULL &&
        player_one_render_state != NULL &&
        player_two_render_state != NULL &&
        player_one_frame_valid &&
        player_two_frame_valid &&
        frame_cache_device != NULL &&
        player_one_frame_texture != NULL &&
        player_two_frame_texture != NULL &&
        player_one_frame_surface != NULL &&
        player_two_frame_surface != NULL;
}

BOOL SudekiMpSplitScreenQuickMenuLiveViewAccepted(
    BOOL isolation_requested,
    BOOL resources_ready,
    BOOL player_two_requested_before_apply,
    BOOL player_two_rendered
) {
    return isolation_requested &&
        resources_ready &&
        (!player_two_requested_before_apply || player_two_rendered);
}

unsigned int SudekiMpSplitScreenQuickMenuIsolationBeginState(
    unsigned int state,
    BOOL was_visible,
    BOOL visible,
    BOOL eligible_on_rising_edge
) {
    BOOL rising_edge = visible && !was_visible;

    if (state > SUDEKIMP_QUICK_MENU_ISOLATION_TAIL) {
        return visible ? SUDEKIMP_QUICK_MENU_ISOLATION_FAILED :
            SUDEKIMP_QUICK_MENU_ISOLATION_IDLE;
    }
    if (rising_edge) {
        return eligible_on_rising_edge ?
            SUDEKIMP_QUICK_MENU_ISOLATION_ACTIVE :
            SUDEKIMP_QUICK_MENU_ISOLATION_FAILED;
    }
    if (!visible && was_visible) {
        return state == SUDEKIMP_QUICK_MENU_ISOLATION_ACTIVE ?
            SUDEKIMP_QUICK_MENU_ISOLATION_TAIL :
            SUDEKIMP_QUICK_MENU_ISOLATION_IDLE;
    }
    if (visible) {
        return state == SUDEKIMP_QUICK_MENU_ISOLATION_ACTIVE ||
            state == SUDEKIMP_QUICK_MENU_ISOLATION_FAILED ? state :
            SUDEKIMP_QUICK_MENU_ISOLATION_FAILED;
    }
    return state == SUDEKIMP_QUICK_MENU_ISOLATION_TAIL ? state :
        SUDEKIMP_QUICK_MENU_ISOLATION_IDLE;
}

unsigned int SudekiMpSplitScreenQuickMenuIsolationEndState(
    unsigned int state,
    BOOL quick_menu_submit_seen,
    BOOL owner_frame_captured
) {
    if (state == SUDEKIMP_QUICK_MENU_ISOLATION_TAIL &&
        !quick_menu_submit_seen && owner_frame_captured) {
        return SUDEKIMP_QUICK_MENU_ISOLATION_IDLE;
    }
    return state;
}

unsigned int SudekiMpSplitScreenQuickMenuIsolationCancelState(
    BOOL quick_menu_visible
) {
    return quick_menu_visible ? SUDEKIMP_QUICK_MENU_ISOLATION_FAILED :
        SUDEKIMP_QUICK_MENU_ISOLATION_IDLE;
}

BOOL SudekiMpSplitScreenQuickMenuPinnedViewIsPlayerTwo(
    BOOL isolation_in_progress,
    BOOL owner_valid,
    BOOL owner_player_two,
    BOOL fallback_player_two
) {
    return isolation_in_progress && owner_valid ? owner_player_two :
        fallback_player_two;
}

BOOL SudekiMpSplitScreenQuickMenuSubmitShouldBeSuppressed(
    BOOL isolation_in_progress,
    BOOL render_phase_confirmed,
    BOOL owner_valid,
    BOOL owner_player_two,
    BOOL player_two_rendered
) {
    /* The native Quick Menu root mixes current-frame presentation with text
     * queued for a later CUIScene flush.  Its owning viewport is pinned for
     * the complete open/tail session, so reject any non-owner invocation and
     * let both portions land in consecutive frames of the same owner cache. */
    return isolation_in_progress &&
        render_phase_confirmed &&
        owner_valid &&
        player_two_rendered != owner_player_two;
}

BOOL SudekiMpSplitScreenQuickMenuOwnerCaptureAdvanced(
    BOOL isolation_in_progress,
    BOOL owner_valid,
    BOOL owner_player_two,
    BOOL player_two_rendered,
    BOOL capture_allowed,
    BOOL compose_succeeded
) {
    return isolation_in_progress && owner_valid && capture_allowed &&
        compose_succeeded && player_two_rendered == owner_player_two;
}

BOOL SudekiMpSplitScreenMinimapOwnerIsPlayerTwo(
    BOOL enabled,
    BOOL update_owner_valid,
    BOOL update_player_two
) {
    return enabled && update_owner_valid && update_player_two;
}

BOOL SudekiMpSplitScreenMinimapCaptureAllowed(
    BOOL enabled,
    BOOL update_owner_valid,
    BOOL update_player_two,
    BOOL rendered_player_two
) {
    if (!enabled) {
        return TRUE;
    }
    return update_owner_valid &&
        update_player_two == rendered_player_two;
}

static int current_spirit_presentation_state(void);

static BOOL quick_menu_render_submit_would_submit(void *quick_menu) {
    const uint8_t *object = (const uint8_t *)quick_menu;

    return readable_memory(object, 0xffu) &&
        *(const uint32_t *)(object + 0x38u) == 0u &&
        object[0xfeu] == 0u;
}

static void trace_quick_menu_owner_state(void *quick_menu) {
    static const unsigned int offsets[5] = {
        0x1f4u, /* selected skill/item */
        0x204u, /* menu mode */
        0x208u, /* menu resource/controller */
        0x214u, /* native active-character owner */
        0x218u  /* owner-side resource/selection state */
    };
    uint32_t values[5];
    unsigned int index;
    unsigned int owner_p1_offset;
    unsigned int owner_p2_offset;
    unsigned int nested_p1_offset = 0xffffffffu;
    unsigned int nested_p2_offset = 0xffffffffu;
    unsigned int menu_p1_offset;
    unsigned int menu_p2_offset;
    unsigned int resource_p1_offset;
    unsigned int resource_p2_offset;
    unsigned int aux_p1_offset;
    unsigned int aux_p2_offset;
    unsigned int graph_first_p1_offset = 0xffffffffu;
    unsigned int graph_second_p1_offset = 0xffffffffu;
    unsigned int graph_first_p2_offset = 0xffffffffu;
    unsigned int graph_second_p2_offset = 0xffffffffu;
    uint8_t *controller;
    void *owner;
    void *nested;
    void *controller_target = NULL;

    if (quick_menu == NULL || !readable_memory(quick_menu, 0x21cu)) {
        return;
    }
    for (index = 0u; index < 5u; ++index) {
        values[index] = *(uint32_t *)((uint8_t *)quick_menu + offsets[index]);
    }
    if (quick_menu_owner_trace_valid &&
        memcmp(values, quick_menu_owner_trace_values,
            sizeof(values)) == 0) {
        return;
    }
    memcpy(quick_menu_owner_trace_values, values, sizeof(values));
    quick_menu_owner_trace_valid = TRUE;
    owner = *(void **)((uint8_t *)quick_menu + 0x214u);
    owner_p1_offset = quick_menu_find_character_reference(
        owner, 0x500u, player_one_character);
    owner_p2_offset = quick_menu_find_character_reference(
        owner, 0x500u, player_two_character);
    menu_p1_offset = quick_menu_find_character_reference(
        quick_menu, 0x21cu, player_one_character);
    menu_p2_offset = quick_menu_find_character_reference(
        quick_menu, 0x21cu, player_two_character);
    resource_p1_offset = quick_menu_find_character_reference(
        (void *)(uintptr_t)values[2], 0x500u, player_one_character);
    resource_p2_offset = quick_menu_find_character_reference(
        (void *)(uintptr_t)values[2], 0x500u, player_two_character);
    aux_p1_offset = quick_menu_find_character_reference(
        (void *)(uintptr_t)values[4], 0x500u, player_one_character);
    aux_p2_offset = quick_menu_find_character_reference(
        (void *)(uintptr_t)values[4], 0x500u, player_two_character);
    (void)quick_menu_find_character_graph_reference(
        quick_menu, 0x21cu, player_one_character, 2u,
        &graph_first_p1_offset, &graph_second_p1_offset);
    (void)quick_menu_find_character_graph_reference(
        quick_menu, 0x21cu, player_two_character, 2u,
        &graph_first_p2_offset, &graph_second_p2_offset);
    if (readable_memory(owner, 0x3c4u)) {
        nested = *(void **)((uint8_t *)owner + 0x3c0u);
        nested_p1_offset = quick_menu_find_character_reference(
            nested, 0x500u, player_one_character);
        nested_p2_offset = quick_menu_find_character_reference(
            nested, 0x500u, player_two_character);
    }
    if (game_base != NULL && readable_memory(
            game_base + RVA_CHARACTER_CONTROLLER_GLOBAL,
            sizeof(controller))) {
        controller = *(uint8_t **)(game_base +
            RVA_CHARACTER_CONTROLLER_GLOBAL);
        if (readable_memory(controller, CONTROLLER_TARGET_OFFSET +
                sizeof(controller_target))) {
            controller_target = *(void **)(controller +
                CONTROLLER_TARGET_OFFSET);
        }
    }
    SudekiMpLogFormat(
        "split_screen_render event=quick_menu_owner_trace phase=%s quick_menu=0x%08lx owner=0x%08lx controller_target=0x%08lx selected=0x%08lx mode=0x%08lx resource=0x%08lx owner_state=0x%08lx owner_aux=0x%08lx player_one=0x%08lx player_two=0x%08lx owner_p1_offset=0x%08lx owner_p2_offset=0x%08lx nested_p1_offset=0x%08lx nested_p2_offset=0x%08lx menu_p1_offset=0x%08lx menu_p2_offset=0x%08lx resource_p1_offset=0x%08lx resource_p2_offset=0x%08lx aux_p1_offset=0x%08lx aux_p2_offset=0x%08lx graph_p1=0x%08lx:0x%08lx graph_p2=0x%08lx:0x%08lx policy=read_only_native_owner_field_inventory_before_virtualization\r\n",
        rendered_player_two_this_frame ? "player_two_submit" :
            "player_one_submit",
        (unsigned long)(uintptr_t)quick_menu,
        (unsigned long)values[3],
        (unsigned long)(uintptr_t)controller_target,
        (unsigned long)values[0],
        (unsigned long)values[1],
        (unsigned long)values[2],
        (unsigned long)values[4],
        (unsigned long)*(uint32_t *)((uint8_t *)quick_menu + 0x20cu),
        (unsigned long)(uintptr_t)player_one_character,
        (unsigned long)(uintptr_t)player_two_character,
        (unsigned long)owner_p1_offset,
        (unsigned long)owner_p2_offset,
        (unsigned long)nested_p1_offset,
        (unsigned long)nested_p2_offset,
        (unsigned long)menu_p1_offset,
        (unsigned long)menu_p2_offset,
        (unsigned long)resource_p1_offset,
        (unsigned long)resource_p2_offset,
        (unsigned long)aux_p1_offset,
        (unsigned long)aux_p2_offset,
        (unsigned long)graph_first_p1_offset,
        (unsigned long)graph_second_p1_offset,
        (unsigned long)graph_first_p2_offset,
        (unsigned long)graph_second_p2_offset
    );
}

static void reset_quick_menu_owner_session(void) {
    if ((quick_menu_session.held_party_source[0] != 0u ||
         quick_menu_session.held_party_source[1] != 0u ||
         quick_menu_session.held_party_source[2] != 0u) &&
        quick_menu_party_pointer_cleanup != NULL) {
        quick_menu_party_pointer_cleanup(
            quick_menu_session.held_party_source);
    }
    ZeroMemory(&quick_menu_session, sizeof(quick_menu_session));
    quick_menu_session.phase = SUDEKIMP_QUICK_MENU_SESSION_IDLE;
    quick_menu_owner_session_valid = FALSE;
    quick_menu_owner_character = NULL;
    quick_menu_owner_player_two = FALSE;
    quick_menu_owner_session_logged = FALSE;
}

static uint8_t *quick_menu_current_group(void) {
    uint8_t *group;

    if (game_base == NULL || !readable_memory(
            game_base + RVA_ACTIVE_GROUP_GLOBAL,
            sizeof(group))) {
        return NULL;
    }
    group = *(uint8_t **)(game_base + RVA_ACTIVE_GROUP_GLOBAL);
    return group != NULL && readable_memory(
            group + PARTY_SLOT_ZERO_OFFSET,
            PARTY_SLOT_COUNT * PARTY_SLOT_STRIDE) ? group : NULL;
}

static uint8_t *quick_menu_unique_party_source(
    uint8_t *group,
    void *actor
) {
    uint8_t *resolved = NULL;
    unsigned int matches = 0u;
    unsigned int index;

    if (group == NULL || actor == NULL || !readable_memory(
            group + PARTY_SLOT_ZERO_OFFSET,
            PARTY_SLOT_COUNT * PARTY_SLOT_STRIDE)) {
        return NULL;
    }
    for (index = 0u; index < PARTY_SLOT_COUNT; ++index) {
        uint8_t *candidate = group + PARTY_SLOT_ZERO_OFFSET +
            index * PARTY_SLOT_STRIDE;
        if (*(void **)candidate == actor) {
            resolved = candidate;
            ++matches;
        }
    }
    return matches == 1u ? resolved : NULL;
}

static BOOL quick_menu_retain_party_source(
    const void *source,
    const void *actor
) {
    void *destination = quick_menu_session.held_party_source;
    void *native_source = (void *)source;

    if (source == NULL || actor == NULL ||
        original_hud_party_pointer_copy == NULL ||
        quick_menu_party_pointer_cleanup == NULL ||
        quick_menu_session.held_party_source[0] != 0u ||
        quick_menu_session.held_party_source[1] != 0u ||
        quick_menu_session.held_party_source[2] != 0u) {
        return FALSE;
    }
    __asm__ volatile(
        "call *%[copy]"
        : "+a"(destination), "+c"(native_source)
        : [copy] "r"(original_hud_party_pointer_copy)
        : "edx", "memory", "cc"
    );
    if ((void *)(uintptr_t)quick_menu_session.held_party_source[0] != actor) {
        quick_menu_party_pointer_cleanup(
            quick_menu_session.held_party_source);
        ZeroMemory(
            quick_menu_session.held_party_source,
            sizeof(quick_menu_session.held_party_source));
        return FALSE;
    }
    return TRUE;
}

static uint8_t *quick_menu_held_party_source(void) {
    return (void *)(uintptr_t)quick_menu_session.held_party_source[0] ==
            quick_menu_session.actor ?
        (uint8_t *)quick_menu_session.held_party_source : NULL;
}

static void quick_menu_publish_owner_session(void) {
    quick_menu_owner_session_valid =
        quick_menu_session.phase != SUDEKIMP_QUICK_MENU_SESSION_IDLE;
    quick_menu_owner_character = quick_menu_session.actor;
    quick_menu_owner_player_two = quick_menu_session.owner_seat == 1u;
    if (quick_menu_owner_session_valid && !quick_menu_owner_session_logged) {
        quick_menu_owner_session_logged = TRUE;
        SudekiMpLogFormat(
            "split_screen_render event=quick_menu_owner_session owner=player_%u serial=%lu character=0x%08lx actor_generation=%lu input=0x%08lx camera=0x%08lx render_state=0x%08lx policy=serialized_native_skills_owner_pinned_no_group_or_controller_write\r\n",
            quick_menu_session.owner_seat + 1u,
            (unsigned long)quick_menu_session.serial,
            (unsigned long)(uintptr_t)quick_menu_session.actor,
            (unsigned long)quick_menu_session.actor_generation,
            (unsigned long)(uintptr_t)quick_menu_session.input_identity,
            (unsigned long)(uintptr_t)quick_menu_session.camera,
            (unsigned long)(uintptr_t)quick_menu_session.render_state
        );
    }
}

static BOOL quick_menu_capture_seat_session(
    unsigned int seat_index,
    SudekiMpQuickMenuSessionPhase phase
) {
    SudekiMpPlayerStatehood *statehood;
    const SudekiMpPlayerLease *lease;
    SudekiMpPlayerCombatSnapshot combat;
    uint8_t *group;
    void *actor;
    void *camera;
    void *render_state;
    void *input_identity;
    void *native_menu;
    uint8_t *party_source;

    if (seat_index >= 2u || !split_screen_render_installed ||
        !runtime_split_enabled || !quick_menu_live_player_two_ready()) {
        return FALSE;
    }
    statehood = SudekiMpPlayerStatehoodRuntime();
    lease = statehood == NULL ? NULL : &statehood->players[seat_index];
    if (lease == NULL || !lease->human_present || lease->actor == 0u ||
        lease->actor_generation == 0u ||
        !SudekiMpCombatContextGetSnapshot(seat_index, &combat)) {
        return FALSE;
    }
    actor = seat_index == 0u ? player_one_character : player_two_character;
    camera = seat_index == 0u ? player_one_camera : player_two_camera;
    render_state = seat_index == 0u ?
        player_one_render_state : player_two_render_state;
    input_identity = combat.input_source;
    if (actor == NULL || camera == NULL || render_state == NULL ||
        input_identity == NULL || lease->actor != (uintptr_t)actor ||
        combat.character != actor || combat.viewport_camera != camera ||
        combat.render_state != render_state ||
        (seat_index == 0u &&
         combat.input_source_kind !=
            SUDEKIMP_COMBAT_INPUT_NATIVE_CONTROLLER) ||
        (seat_index == 1u &&
         (combat.input_source_kind !=
                SUDEKIMP_COMBAT_INPUT_EXTERNAL_BRIDGE ||
          !SudekiMpControlSeparationPlayerTwoActive() ||
          !SudekiMpControlSeparationInputReady() ||
          input_identity != (void *)SudekiMpInputBridgeIdentity()))) {
        return FALSE;
    }
    group = quick_menu_current_group();
    native_menu = quick_menu_singleton();
    party_source = quick_menu_unique_party_source(group, actor);
    if (group == NULL || native_menu == NULL ||
        party_source == NULL) {
        return FALSE;
    }
    ++quick_menu_next_serial;
    if (quick_menu_next_serial == 0u) {
        ++quick_menu_next_serial;
    }
    ZeroMemory(&quick_menu_session, sizeof(quick_menu_session));
    quick_menu_session.phase = phase;
    quick_menu_session.owner_seat = seat_index;
    quick_menu_session.serial = quick_menu_next_serial;
    quick_menu_session.actor = actor;
    quick_menu_session.actor_generation = lease->actor_generation;
    quick_menu_session.input_identity = input_identity;
    /* The legacy bridge exposes a stable identity and live-ready predicate,
     * but no generation. Zero records that limitation honestly. */
    quick_menu_session.input_generation = 0u;
    quick_menu_session.camera = camera;
    quick_menu_session.render_state = render_state;
    quick_menu_session.native_menu = native_menu;
    quick_menu_session.group = group;
    if (!quick_menu_retain_party_source(party_source, actor)) {
        reset_quick_menu_owner_session();
        return FALSE;
    }
    quick_menu_publish_owner_session();
    return TRUE;
}

static BOOL quick_menu_session_lease_valid(
    BOOL require_open,
    BOOL require_skills
) {
    SudekiMpPlayerStatehood *statehood;
    const SudekiMpPlayerLease *lease;
    SudekiMpPlayerCombatSnapshot combat;
    uint8_t *menu = (uint8_t *)quick_menu_session.native_menu;
    uint8_t *group;
    unsigned int seat_index = quick_menu_session.owner_seat;

    if (quick_menu_session.phase == SUDEKIMP_QUICK_MENU_SESSION_IDLE ||
        seat_index >= 2u || !quick_menu_owner_session_valid ||
        menu == NULL || menu != quick_menu_singleton() ||
        !readable_memory(
            menu,
            QUICK_MENU_CATEGORY_OFFSET + sizeof(uint32_t))) {
        return FALSE;
    }
    statehood = SudekiMpPlayerStatehoodRuntime();
    lease = statehood == NULL ? NULL : &statehood->players[seat_index];
    group = quick_menu_current_group();
    if (lease == NULL || !lease->human_present ||
        lease->actor != (uintptr_t)quick_menu_session.actor ||
        lease->actor_generation != quick_menu_session.actor_generation ||
        group == NULL || group != quick_menu_session.group ||
        quick_menu_unique_party_source(group, quick_menu_session.actor) == NULL ||
        !SudekiMpCombatContextGetSnapshot(seat_index, &combat) ||
        combat.character != quick_menu_session.actor ||
        combat.input_source != quick_menu_session.input_identity ||
        combat.viewport_camera != quick_menu_session.camera ||
        combat.render_state != quick_menu_session.render_state ||
        (seat_index == 0u &&
         (quick_menu_session.actor != player_one_character ||
          quick_menu_session.camera != player_one_camera ||
          quick_menu_session.render_state != player_one_render_state ||
          combat.input_source_kind !=
            SUDEKIMP_COMBAT_INPUT_NATIVE_CONTROLLER)) ||
        (seat_index == 1u &&
         (quick_menu_session.actor != player_two_character ||
          quick_menu_session.camera != player_two_camera ||
          quick_menu_session.render_state != player_two_render_state ||
          combat.input_source_kind !=
            SUDEKIMP_COMBAT_INPUT_EXTERNAL_BRIDGE ||
          !SudekiMpControlSeparationPlayerTwoActive() ||
          !SudekiMpControlSeparationInputReady() ||
          quick_menu_session.input_identity !=
            (void *)SudekiMpInputBridgeIdentity())) ||
        (require_open && menu[QUICK_MENU_ACTIVE_OFFSET] == 0u) ||
        (require_skills && seat_index == 1u && *(uint32_t *)(menu +
            QUICK_MENU_CATEGORY_OFFSET) != QUICK_MENU_SKILLS_CATEGORY)) {
        return FALSE;
    }
    return TRUE;
}

static void quick_menu_quarantine(const char *reason) {
    if (quick_menu_session.phase == SUDEKIMP_QUICK_MENU_SESSION_IDLE ||
        quick_menu_session.phase == SUDEKIMP_QUICK_MENU_SESSION_CLOSING) {
        return;
    }
    quick_menu_session.phase = SUDEKIMP_QUICK_MENU_SESSION_QUARANTINED;
    if (!quick_menu_session.quarantine_logged) {
        quick_menu_session.quarantine_logged = TRUE;
        SudekiMpLogFormat(
            "split_screen_render event=quick_menu_owner_session phase=quarantined owner=player_%u serial=%lu reason=%s policy=close_before_next_render_held_owner_reference_preferred_original_only_after_actor_invalidation\r\n",
            quick_menu_session.owner_seat + 1u,
            (unsigned long)quick_menu_session.serial,
            reason == NULL ? "lease_invalid" : reason
        );
    }
}

static BOOL quick_menu_seat_one_owns_native_input(void) {
    return quick_menu_owner_session_valid &&
        quick_menu_session.owner_seat == 1u &&
        quick_menu_session.phase != SUDEKIMP_QUICK_MENU_SESSION_IDLE;
}

static void quick_menu_close_quarantined(void) {
    if (quick_menu_session.phase !=
            SUDEKIMP_QUICK_MENU_SESSION_QUARANTINED ||
        quick_menu_close == NULL) {
        return;
    }
    quick_menu_session.phase = SUDEKIMP_QUICK_MENU_SESSION_CLOSING;
    if (genuine_quick_menu_visible()) {
        (void)quick_menu_close();
    }
}

static void quick_menu_latch_owner_from_controller(void) {
    uint8_t *controller;
    void *target = NULL;

    if (quick_menu_owner_session_valid) {
        if (quick_menu_session.phase ==
                SUDEKIMP_QUICK_MENU_SESSION_OPEN_REQUESTED) {
            if (quick_menu_session_lease_valid(TRUE, TRUE)) {
                quick_menu_session.phase =
                    SUDEKIMP_QUICK_MENU_SESSION_NATIVE_OPEN;
            } else {
                quick_menu_quarantine("pending_owner_lease_invalid_on_open");
            }
        }
        return;
    }
    if (game_base == NULL || !readable_memory(
            game_base + RVA_CHARACTER_CONTROLLER_GLOBAL,
            sizeof(controller))) {
        return;
    }
    controller = *(uint8_t **)(game_base + RVA_CHARACTER_CONTROLLER_GLOBAL);
    if (!readable_memory(
            controller,
            CONTROLLER_TARGET_OFFSET + sizeof(target))) {
        return;
    }
    target = *(void **)(controller + CONTROLLER_TARGET_OFFSET);
    /* A native open without an explicit request is owned by P1. Controller
     * target is only an identity check; it is never changed to manufacture
     * ownership for another seat. */
    if (target != player_one_character ||
        !quick_menu_capture_seat_session(
            0u,
            SUDEKIMP_QUICK_MENU_SESSION_NATIVE_OPEN)) {
        return;
    }
}

static void quick_menu_service_owner_session(BOOL native_visible) {
    if (!quick_menu_owner_session_valid ||
        quick_menu_session.phase == SUDEKIMP_QUICK_MENU_SESSION_IDLE) {
        return;
    }
    if (quick_menu_session.phase ==
            SUDEKIMP_QUICK_MENU_SESSION_NATIVE_OPEN) {
        if (!native_visible) {
            quick_menu_session.phase = SUDEKIMP_QUICK_MENU_SESSION_CLOSING;
        } else if (!quick_menu_session_lease_valid(TRUE, TRUE)) {
            quick_menu_quarantine(
                "frame_owner_lease_or_skills_category_invalid");
        }
    } else if (quick_menu_session.phase ==
                   SUDEKIMP_QUICK_MENU_SESSION_OPEN_REQUESTED &&
               native_visible) {
        quick_menu_latch_owner_from_controller();
    }
    quick_menu_close_quarantined();
}

BOOL SudekiMpSplitScreenQuickMenuRequest(unsigned int seat_index) {
    if (seat_index >= 2u || quick_menu_start == NULL ||
        quick_menu_session.phase != SUDEKIMP_QUICK_MENU_SESSION_IDLE ||
        genuine_quick_menu_visible() ||
        (seat_index == 1u &&
         (!runtime_authorized_at_render_start ||
          !SudekiMpSplitScreenRuntimeAuthorized() ||
          pc_quit_screen_visible() ||
          SudekiMpSplitScreenSharedInteractionModalActive() ||
          current_spirit_presentation_state() != 0 ||
          player_two_temporary_camera_policy ==
            SUDEKIMP_TEMP_CAMERA_SHARED_FULL_WIDTH)) ||
        !quick_menu_capture_seat_session(
            seat_index,
            SUDEKIMP_QUICK_MENU_SESSION_OPEN_REQUESTED)) {
        return FALSE;
    }
    quick_menu_start();
    if (quick_menu_session.phase ==
            SUDEKIMP_QUICK_MENU_SESSION_QUARANTINED) {
        quick_menu_close_quarantined();
        reset_quick_menu_owner_session();
        return FALSE;
    }
    if (!genuine_quick_menu_visible() ||
        !quick_menu_session_lease_valid(TRUE, TRUE)) {
        if (genuine_quick_menu_visible() && quick_menu_close != NULL) {
            quick_menu_session.phase = SUDEKIMP_QUICK_MENU_SESSION_CLOSING;
            (void)quick_menu_close();
        }
        reset_quick_menu_owner_session();
        return FALSE;
    }
    quick_menu_session.phase = SUDEKIMP_QUICK_MENU_SESSION_NATIVE_OPEN;
    SudekiMpLogFormat(
        "split_screen_render event=quick_menu_request owner=player_%u serial=%lu status=opened category=skills policy=one_native_singleton_no_queue\r\n",
        seat_index + 1u,
        (unsigned long)quick_menu_session.serial
    );
    return TRUE;
}

BOOL SudekiMpSplitScreenQuickMenuActive(unsigned int seat_index) {
    if (seat_index >= 2u || !quick_menu_owner_session_valid ||
        quick_menu_session.owner_seat != seat_index ||
        quick_menu_session.phase == SUDEKIMP_QUICK_MENU_SESSION_IDLE) {
        return FALSE;
    }
    if ((quick_menu_session.phase ==
            SUDEKIMP_QUICK_MENU_SESSION_OPEN_REQUESTED ||
         quick_menu_session.phase ==
            SUDEKIMP_QUICK_MENU_SESSION_NATIVE_OPEN) &&
        genuine_quick_menu_visible() &&
        !quick_menu_session_lease_valid(TRUE, TRUE)) {
        quick_menu_quarantine("active_owner_lease_or_skills_category_invalid");
    }
    /* Ownership remains modal through quarantine/close so gameplay cannot
     * resume behind a still-visible singleton.  Submit separately requires
     * NATIVE_OPEN and therefore stays fail closed. */
    return TRUE;
}

BOOL SudekiMpSplitScreenQuickMenuAnyActive(void) {
    /* A native P1 open can occur during the controller update before the
     * following RenderStart has captured its serialized owner.  Treat the
     * singleton's exact visible edge as modal immediately so P2 cannot act
     * behind the owner-pinned viewport during that one-frame handoff. */
    return genuine_quick_menu_visible() ||
        (quick_menu_owner_session_valid &&
         quick_menu_session.phase != SUDEKIMP_QUICK_MENU_SESSION_IDLE);
}

BOOL SudekiMpSplitScreenQuickMenuSubmit(
    unsigned int seat_index,
    SudekiMpSplitScreenQuickMenuAction action
) {
    unsigned int command;
    uint8_t handled;
    BOOL close_requested = FALSE;

    switch (action) {
    case SUDEKIMP_QUICK_MENU_ACTION_CONFIRM:
        command = QUICK_MENU_COMMAND_CONFIRM;
        break;
    case SUDEKIMP_QUICK_MENU_ACTION_CANCEL:
        command = QUICK_MENU_COMMAND_CANCEL;
        close_requested = TRUE;
        break;
    case SUDEKIMP_QUICK_MENU_ACTION_SECONDARY:
        /* QuickMenu command 2 reaches the same activation branch as confirm;
         * it is not an independent Skills action. */
        return FALSE;
    case SUDEKIMP_QUICK_MENU_ACTION_UP:
        command = QUICK_MENU_COMMAND_UP;
        break;
    case SUDEKIMP_QUICK_MENU_ACTION_DOWN:
        command = QUICK_MENU_COMMAND_DOWN;
        break;
    default:
        return FALSE;
    }
    if (!SudekiMpSplitScreenQuickMenuActive(seat_index) ||
        quick_menu_session.phase != SUDEKIMP_QUICK_MENU_SESSION_NATIVE_OPEN ||
        !genuine_quick_menu_visible() ||
        (close_requested ? quick_menu_close == NULL :
            original_quick_menu_input == NULL)) {
        return FALSE;
    }
    if (close_requested) {
        /* Command 11's release branch is animation-state dependent.  The
         * native close helper performs its event-4 lifecycle and close
         * synchronously, while CLOSING keeps every owner copy pinned. */
        quick_menu_session.phase = SUDEKIMP_QUICK_MENU_SESSION_CLOSING;
        (void)quick_menu_close();
        return TRUE;
    }
    handled = original_quick_menu_input(
        quick_menu_session.native_menu,
        QUICK_MENU_INPUT_EVENT_DOWN,
        command,
        1u
    );
    (void)original_quick_menu_input(
        quick_menu_session.native_menu,
        QUICK_MENU_INPUT_EVENT_UP,
        command,
        0u
    );
    if (!genuine_quick_menu_visible()) {
        quick_menu_session.phase = SUDEKIMP_QUICK_MENU_SESSION_CLOSING;
    }
    return handled != 0u;
}

static uint8_t SUDEKIMP_THISCALL route_quick_menu_input(
    void *quick_menu,
    unsigned int event_kind,
    unsigned int command,
    unsigned int value
) {
    if (quick_menu == quick_menu_session.native_menu &&
        quick_menu_seat_one_owns_native_input() &&
        (event_kind == QUICK_MENU_INPUT_EVENT_DOWN ||
         event_kind == QUICK_MENU_INPUT_EVENT_UP ||
         event_kind == QUICK_MENU_INPUT_EVENT_POINTER)) {
        return 1u;
    }
    return original_quick_menu_input == NULL ? 0u :
        original_quick_menu_input(
            quick_menu,
            event_kind,
            command,
            value
        );
}

BOOL SudekiMpSplitScreenQuickMenuNativeToggleSuppressed(void) {
    return quick_menu_seat_one_owns_native_input();
}

__attribute__((naked, noinline, used))
static void quick_menu_native_toggle_entry(void) {
    __asm__ volatile(
        "pushl %eax\n\t"
        "call _SudekiMpSplitScreenQuickMenuNativeToggleSuppressed\n\t"
        "testl %eax, %eax\n\t"
        "popl %eax\n\t"
        "jnz 1f\n\t"
        "call *_original_quick_menu_native_toggle\n\t"
        "ret\n\t"
        "1:\n\t"
        "xorl %eax, %eax\n\t"
        "ret\n\t"
    );
}

static void *quick_menu_owner_source_dispatch(
    void *source,
    BOOL default_recipient
) {
    uint8_t *group;
    uint8_t *owner_source;
    uint8_t *menu = (uint8_t *)quick_menu_session.native_menu;

    if (!quick_menu_owner_session_valid ||
        quick_menu_session.phase == SUDEKIMP_QUICK_MENU_SESSION_IDLE) {
        return source;
    }
    if (quick_menu_session.owner_seat == 0u) {
        return source;
    }
    if (default_recipient &&
        (!readable_memory(
            menu,
            QUICK_MENU_DEFAULT_RECIPIENT_OFFSET + sizeof(int32_t)) ||
         *(int32_t *)(menu + QUICK_MENU_DEFAULT_RECIPIENT_OFFSET) >= 0)) {
        return source;
    }
    group = quick_menu_current_group();
    owner_source = quick_menu_unique_party_source(
        group,
        quick_menu_session.actor
    );
    if (group == NULL || group != quick_menu_session.group ||
        source != group + PARTY_SLOT_ZERO_OFFSET) {
        quick_menu_quarantine("native_owner_source_or_group_mismatch");
    }
    if (owner_source == NULL) {
        quick_menu_quarantine("owner_actor_party_slot_not_unique");
        owner_source = quick_menu_held_party_source();
    }
    /* Native Close clears menu+0x29 before its SetUIActive(false) owner copy.
     * Therefore this path deliberately depends only on the captured
     * actor/group/unique-slot identity, never active/category/input/view
     * state.  CLOSING and QUARANTINED still complete against the same real
     * 12-byte party slot instead of falling back to the front actor. */
    if (owner_source == NULL) {
        quick_menu_quarantine("held_owner_reference_invalid");
        /* Actor destruction invalidates every intrusive observer.  The menu
         * is already quarantined for close at the next pre-render boundary;
         * preserve native memory safety for this in-flight call. */
        return source;
    }
    return owner_source;
}

void *SudekiMpSplitScreenQuickMenuOwnerSourceDispatch(void *source) {
    return quick_menu_owner_source_dispatch(source, FALSE);
}

void *SudekiMpSplitScreenQuickMenuDefaultSourceDispatch(void *source) {
    return quick_menu_owner_source_dispatch(source, TRUE);
}

__attribute__((naked, noinline, used))
static void quick_menu_owner_pointer_copy_entry(void) {
    __asm__ volatile(
        "pushl %eax\n\t"
        "pushl %ecx\n\t"
        "call _SudekiMpSplitScreenQuickMenuOwnerSourceDispatch\n\t"
        "addl $4, %esp\n\t"
        "movl %eax, %ecx\n\t"
        "popl %eax\n\t"
        "call *_original_hud_party_pointer_copy\n\t"
        "ret\n\t"
    );
}

__attribute__((naked, noinline, used))
static void quick_menu_owner_default_pointer_copy_entry(void) {
    __asm__ volatile(
        "pushl %eax\n\t"
        "pushl %ecx\n\t"
        "call _SudekiMpSplitScreenQuickMenuDefaultSourceDispatch\n\t"
        "addl $4, %esp\n\t"
        "movl %eax, %ecx\n\t"
        "popl %eax\n\t"
        "call *_original_hud_party_pointer_copy\n\t"
        "ret\n\t"
    );
}

static BOOL install_quick_menu_owner_hooks(void) {
    unsigned int index;

    if (!SudekiMpInstallPointerHook(
            &quick_menu_input_hook,
            (void **)(game_base + RVA_QUICK_MENU_INPUT_VTABLE_SLOT),
            original_quick_menu_input,
            route_quick_menu_input) ||
        !SudekiMpInstallRelativeCallHook(
            &quick_menu_native_toggle_hook,
            game_base + RVA_QUICK_MENU_NATIVE_TOGGLE_CALL,
            original_quick_menu_native_toggle,
            quick_menu_native_toggle_entry)) {
        return FALSE;
    }
    for (index = 0u; index < QUICK_MENU_OWNER_COPY_HOOK_COUNT; ++index) {
        if (!SudekiMpInstallRelativeCallHook(
                &quick_menu_owner_copy_hooks[index],
                game_base + quick_menu_owner_copy_call_rvas[index],
                original_hud_party_pointer_copy,
                quick_menu_owner_pointer_copy_entry)) {
            return FALSE;
        }
    }
    return SudekiMpInstallRelativeCallHook(
        &quick_menu_owner_default_recipient_hook,
        game_base + RVA_QUICK_MENU_OWNER_COPY_DEFAULT_RECIPIENT,
        original_hud_party_pointer_copy,
        quick_menu_owner_default_pointer_copy_entry
    );
}

static void SUDEKIMP_THISCALL route_quick_menu_render_submit(
    void *quick_menu
) {
    BOOL singleton_match = quick_menu == quick_menu_singleton();
    BOOL would_submit = singleton_match &&
        quick_menu_render_submit_would_submit(quick_menu);
    BOOL suppress_this_submit = FALSE;

    if (fixed_three_assignment_selected()) {
        original_quick_menu_render_submit(quick_menu);
        return;
    }

    if (would_submit &&
        (genuine_quick_menu_visible() ||
         quick_menu_isolation_active ||
         quick_menu_isolation_tail_active)) {
        quick_menu_latch_owner_from_controller();
        trace_quick_menu_owner_state(quick_menu);
        quick_menu_submit_seen_since_frame_end = TRUE;
        suppress_this_submit =
            SudekiMpSplitScreenQuickMenuSubmitShouldBeSuppressed(
                quick_menu_isolation_active ||
                    quick_menu_isolation_tail_active,
                quick_menu_render_phase_confirmed_this_frame,
                quick_menu_owner_session_valid,
                quick_menu_owner_player_two,
                rendered_player_two_this_frame
            );
    }
    if (would_submit &&
        suppress_this_submit &&
        quick_menu_render_phase_confirmed_this_frame &&
        (quick_menu_isolation_active ||
         quick_menu_isolation_tail_active) &&
        !pc_quit_screen_visible() &&
        current_spirit_presentation_state() == 0) {
        if (!quick_menu_render_submit_isolation_logged) {
            quick_menu_render_submit_isolation_logged = TRUE;
            SudekiMpLogFormat(
                "split_screen_render event=quick_menu_render_submit_isolation phase=active render_submit_rva=0x%08lx render_submit_vtable_slot_rva=0x%08lx policy=allow_owner_only_pin_same_owner_for_current_and_next_frame_payload\r\n",
                (unsigned long)RVA_QUICK_MENU_RENDER_SUBMIT,
                (unsigned long)RVA_QUICK_MENU_RENDER_SUBMIT_VTABLE_SLOT
            );
        }
        return;
    }
    original_quick_menu_render_submit(quick_menu);
}

static int current_spirit_presentation_state(void) {
    uint8_t *manager;

    if (game_base == NULL || !readable_memory(
            game_base + RVA_SPIRIT_STRIKE_MANAGER_GLOBAL,
            sizeof(manager))) {
        return 0;
    }
    manager = *(uint8_t **)(game_base + RVA_SPIRIT_STRIKE_MANAGER_GLOBAL);
    return readable_memory(manager, 0x60u) ? *(int *)(manager + 0x5cu) : 0;
}

BOOL SudekiMpSplitScreenPlayerTwoPerspectivePolicy(
    BOOL runtime_ready,
    BOOL camera_ready,
    BOOL input_ready,
    BOOL presentation_clear,
    BOOL combat_active,
    BOOL actor_matches,
    BOOL lease_matches,
    BOOL ranged_capable
) {
    return runtime_ready && camera_ready && input_ready &&
        presentation_clear && combat_active && actor_matches &&
        lease_matches && ranged_capable;
}

BOOL SudekiMpSplitScreenAdaptiveSeatActivationPolicy(
    BOOL feature_enabled,
    unsigned int active_human_mask,
    BOOL layout_ready,
    unsigned int actor_lease_mask,
    unsigned int camera_lease_mask,
    unsigned int render_state_lease_mask,
    unsigned int hud_lease_mask,
    unsigned int input_lease_mask,
    unsigned int frame_cache_ready_mask,
    BOOL global_presentation_clear
) {
    return SudekiMpLocalViewportActivationPolicy(
        feature_enabled != FALSE,
        active_human_mask,
        layout_ready != FALSE,
        actor_lease_mask,
        camera_lease_mask,
        render_state_lease_mask,
        hud_lease_mask,
        input_lease_mask,
        frame_cache_ready_mask,
        global_presentation_clear != FALSE) ? TRUE : FALSE;
}

BOOL SudekiMpSplitScreenFixedThreeFrameOwnerEvidencePolicy(
    unsigned int rendered_seat,
    unsigned int hud_evidence_seat,
    unsigned int hud_role_mask,
    unsigned int portrait_evidence_seat,
    unsigned int portrait_role_mask,
    unsigned int expected_minimap_update_epoch,
    BOOL minimap_update_valid,
    unsigned int minimap_update_seat,
    unsigned int minimap_update_epoch,
    BOOL minimap_render_valid,
    unsigned int minimap_render_seat,
    BOOL source_failure
) {
    unsigned int required_role_mask;

    if (rendered_seat >= FIXED_THREE_SEAT_COUNT || source_failure) {
        return FALSE;
    }
    required_role_mask = rendered_seat == 0u ? 0x01u : 0x03u;
    return hud_evidence_seat == rendered_seat &&
        hud_role_mask == required_role_mask &&
        portrait_evidence_seat == rendered_seat &&
        portrait_role_mask == required_role_mask &&
        expected_minimap_update_epoch != 0u && minimap_update_valid &&
        minimap_update_seat == rendered_seat &&
        minimap_update_epoch == expected_minimap_update_epoch &&
        minimap_render_valid && minimap_render_seat == rendered_seat;
}

BOOL SudekiMpSplitScreenFixedThreeOrbitInputPolicy(
    BOOL presentation_clear,
    BOOL base_leases_exact,
    BOOL layout_exact,
    unsigned int frame_cache_ready_mask,
    unsigned int frame_owner_evidence_mask,
    BOOL gameplay_input_frozen
) {
    return presentation_clear && base_leases_exact && layout_exact &&
        frame_cache_ready_mask == FIXED_THREE_HUMAN_MASK &&
        frame_owner_evidence_mask == FIXED_THREE_HUMAN_MASK &&
        !gameplay_input_frozen;
}

BOOL SudekiMpSplitScreenPlayerTwoPerspectiveAvailable(void *character) {
    SudekiMpPlayerStatehood *statehood = SudekiMpPlayerStatehoodRuntime();
    const SudekiMpPlayerLease *lease =
        statehood == NULL ? NULL : &statehood->players[1];
    unsigned int player_one_type = 0u;
    unsigned int player_two_type = 0u;
    BOOL roster_type_available = SudekiMpSplitScreenGetRosterTypes(
        &player_one_type, &player_two_type);
    BOOL runtime_ready = split_screen_render_installed &&
        runtime_split_enabled && coop_role_lock_active &&
        coop_roster_participation_requested &&
        !coop_roster_party_transition_active;
    BOOL camera_ready = second_player_camera_enabled &&
        dual_camera_frame_cache_enabled &&
        second_player_controller_camera_enabled &&
        player_two_camera != NULL &&
        writable_memory(player_two_render_state, 0xdcu) &&
        !render_only_swap_active;
    BOOL input_ready = SudekiMpControlSeparationInputReady() &&
        !SudekiMpControlSeparationGameplayInputFrozen();
    BOOL presentation_clear =
        player_two_temporary_camera_policy ==
            SUDEKIMP_TEMP_CAMERA_OUTSIDE &&
        !settled_temporary_zone_active() &&
        !pc_quit_screen_visible() && !quick_menu_visible() &&
        !SudekiMpSplitScreenSharedInteractionModalActive() &&
        player_skill_render_states[0] == NULL &&
        player_skill_render_states[1] == NULL &&
        current_spirit_presentation_state() == 0 &&
        !character_is_using_skill(player_one_character) &&
        !character_is_using_skill(player_two_character);
    BOOL actor_matches = character != NULL &&
        character == player_two_character &&
        character == SudekiMpControlSeparationPlayerTwoCharacter() &&
        SudekiMpControlSeparationPlayerTwoRequested() &&
        SudekiMpControlSeparationPlayerTwoActive();
    BOOL lease_matches = lease != NULL && lease->human_present &&
        lease->actor == (uintptr_t)character &&
        lease->actor_generation != 0u;
    BOOL ranged_capable = roster_type_available &&
        (player_two_type == 0x01u || player_two_type == 0x0eu) &&
        SudekiMpSplitScreenRosterActorIdentityMatches(
            1u, character, player_two_type) &&
        ranged_presentation_parts(character, NULL, NULL, NULL, NULL);

    (void)player_one_type;

    if (fixed_three_assignment_selected()) {
        return FALSE;
    }

    return SudekiMpSplitScreenPlayerTwoPerspectivePolicy(
        runtime_ready,
        camera_ready,
        input_ready,
        presentation_clear,
        active_group_in_combat(),
        actor_matches,
        lease_matches,
        ranged_capable);
}

BOOL SudekiMpSplitScreenTogglePlayerTwoPerspective(
    void *character,
    BOOL *first_person
) {
    SudekiMpPlayerStatehood *statehood;
    const SudekiMpPlayerLease *lease;
    BOOL currently_requested;

    if (first_person == NULL) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    *first_person = player_two_first_person_camera_requested &&
        player_two_perspective_character == character;
    if (!SudekiMpSplitScreenPlayerTwoPerspectiveAvailable(character)) {
        SetLastError(ERROR_INVALID_STATE);
        return FALSE;
    }
    statehood = SudekiMpPlayerStatehoodRuntime();
    lease = &statehood->players[1];
    currently_requested = player_two_first_person_camera_requested &&
        player_two_perspective_character == character &&
        player_two_perspective_actor_generation == lease->actor_generation;
    player_two_first_person_camera_requested = !currently_requested;
    player_two_perspective_character = character;
    player_two_perspective_actor_generation = lease->actor_generation;
    *first_person = player_two_first_person_camera_requested;
    invalidate_dual_frame_cache();
    SudekiMpLogFormat(
        "split_screen_render event=player_two_perspective phase=request "
        "state=%s character=0x%08lx actor_generation=%lu "
        "policy=viewport_two_camera_matrix_only_world_wrapper_retained_no_global_first_person_transition\r\n",
        *first_person ? "first_person" : "third_person",
        (unsigned long)(uintptr_t)character,
        (unsigned long)player_two_perspective_actor_generation);
    return TRUE;
}

BOOL SudekiMpSplitScreenPlayerTwoIsNonCasterDuringSpirit(void *character) {
    return !fixed_three_assignment_selected() &&
        spirit_strike_viewport_effect_isolation_enabled &&
        runtime_split_enabled && dual_camera_frame_cache_enabled &&
        player_one_character != NULL && player_two_character != NULL &&
        character == player_two_character &&
        character != player_one_character &&
        current_spirit_presentation_state() != 0;
}

static BOOL split_skill_realtime_session_active(void) {
    return !fixed_three_assignment_selected() &&
        skill_camera_routing_enabled && runtime_split_enabled &&
        dual_camera_frame_cache_enabled &&
        second_player_camera_manager != NULL &&
        player_two_camera != NULL && player_one_character != NULL &&
        player_two_character != NULL;
}

static BOOL split_skill_slowdown_owner_active(void) {
    return pending_skill_camera_caster != NULL ||
        character_is_using_skill(player_one_character) ||
        character_is_using_skill(player_two_character) ||
        player_skill_render_states[0] != NULL ||
        player_skill_render_states[1] != NULL ||
        current_spirit_presentation_state() != 0;
}

static BOOL set_skill_time_scale_override(BOOL enabled) {
    static const uint32_t native_scale_bits = 0x3d8f5c29u;
    static const uint32_t realtime_scale_bits = 0x3f800000u;
    uint32_t desired_bits = enabled ? realtime_scale_bits : native_scale_bits;
    uint32_t *scale;
    DWORD old_protection;
    DWORD ignored_protection;
    BOOL protection_restored;
    BOOL value_written;

    if (game_base == NULL) {
        return FALSE;
    }
    scale = (uint32_t *)(game_base + RVA_FIXED_ALTERNATE_SPEED);
    if (!readable_memory(scale, sizeof(*scale))) {
        return FALSE;
    }
    if (*scale == desired_bits) {
        skill_time_scale_override_active = enabled;
        return TRUE;
    }
    if (!VirtualProtect(
            scale,
            sizeof(*scale),
            PAGE_EXECUTE_READWRITE,
            &old_protection)) {
        return FALSE;
    }
    *scale = desired_bits;
    FlushInstructionCache(GetCurrentProcess(), scale, sizeof(*scale));
    protection_restored = VirtualProtect(
        scale,
        sizeof(*scale),
        old_protection,
        &ignored_protection
    );
    value_written = *scale == desired_bits;
    if (value_written) {
        skill_time_scale_override_active = enabled;
    }
    return protection_restored && value_written;
}

static void SUDEKIMP_THISCALL route_game_speed_set_mode(
    void *game_speed,
    int requested_mode
) {
    GameSpeedSetModeFunction original =
        (GameSpeedSetModeFunction)set_game_speed_mode_hook.trampoline;
    BOOL split_active = split_skill_realtime_session_active();
    BOOL skill_active = split_skill_slowdown_owner_active();
    /* Native Skill Strikes request mode 2 from the script/event thunk one
     * phase before the character arbiter exposes IsUsingSkill.  Treat that
     * exact mode as a skill-startup request while split co-op is live; later
     * requests can continue to use the explicit owner flags. */
    BOOL pre_skill_startup = requested_mode == 2 && split_active;
    BOOL enable_realtime_scale = requested_mode != 0 && split_active &&
        (skill_active || pre_skill_startup);
    BOOL restore_native_scale = requested_mode == 0 &&
        skill_time_scale_override_active;
    BOOL scale_write_succeeded = TRUE;
    void *caller = __builtin_return_address(0);
    unsigned long caller_rva = game_base != NULL &&
        (uintptr_t)caller >= (uintptr_t)game_base ?
        (unsigned long)((uintptr_t)caller - (uintptr_t)game_base) :
        0xfffffffful;
    int current_mode = -1;
    unsigned int paused = 0xffffffffu;
    const char *outcome = "native_request";

    if (readable_memory(game_speed, 0x29u)) {
        current_mode = *(int *)((uint8_t *)game_speed + 0x20u);
        paused = *(uint8_t *)((uint8_t *)game_speed + 0x28u) != 0u;
    }
    if (enable_realtime_scale) {
        scale_write_succeeded = set_skill_time_scale_override(TRUE);
        outcome = scale_write_succeeded ?
            "native_mode_realtime_scale" :
            "native_mode_scale_override_failed";
    } else if (restore_native_scale) {
        scale_write_succeeded = set_skill_time_scale_override(FALSE);
        outcome = scale_write_succeeded ?
            "native_mode_native_scale_restored" :
            "native_scale_restore_failed";
    }
    if ((requested_mode != 0 && split_active) || restore_native_scale) {
        SudekiMpLogFormat(
            "realtime_skill_combat event=skill_speed_request request=%u caller=0x%08lx caller_rva=0x%08lx game_speed=0x%08lx current_mode=%d requested_mode=%d applied_mode=%d paused=%u p1_using_skill=%u p2_using_skill=%u p1_routed_state=0x%08lx p2_routed_state=0x%08lx spirit_state=%d pre_skill_startup=%u scale_override_active=%u scale_write_succeeded=%u outcome=%s policy=native_mode_handshake_preserved_shared_simulation_scale_1x_pause_byte_unchanged\r\n",
            ++skill_speed_request_sequence,
            (unsigned long)(uintptr_t)caller,
            caller_rva,
            (unsigned long)(uintptr_t)game_speed,
            current_mode,
            requested_mode,
            requested_mode,
            paused,
            character_is_using_skill(player_one_character) ? 1u : 0u,
            character_is_using_skill(player_two_character) ? 1u : 0u,
            (unsigned long)(uintptr_t)player_skill_render_states[0],
            (unsigned long)(uintptr_t)player_skill_render_states[1],
            current_spirit_presentation_state(),
            pre_skill_startup ? 1u : 0u,
            skill_time_scale_override_active ? 1u : 0u,
            scale_write_succeeded ? 1u : 0u,
            outcome
        );
    }
    original(game_speed, requested_mode);
}

static uint8_t *current_spirit_motion_blur_effect(void) {
    uint8_t *scene_manager;
    uint8_t *motion_blur_effect;

    if (game_base == NULL || !readable_memory(
            game_base + RVA_SCENE_MANAGER_GLOBAL,
            sizeof(scene_manager))) {
        return NULL;
    }
    scene_manager = *(uint8_t **)(game_base + RVA_SCENE_MANAGER_GLOBAL);
    if (!readable_memory(scene_manager, 0x74u)) {
        return NULL;
    }
    motion_blur_effect = *(uint8_t **)(scene_manager + 0x70u);
    if (!readable_memory(motion_blur_effect, 0x18u)) {
        return NULL;
    }
    return motion_blur_effect;
}

static void *__attribute__((noinline)) create_spirit_history_surface(void) {
    void *wrapper = NULL;
    void *surface = NULL;
    UINT width = frame_cache_description.width;
    UINT height = frame_cache_description.height;

    if (native_history_resource_factory == NULL || width == 0u ||
        height == 0u) {
        return NULL;
    }
    __asm__ volatile(
        "pushl $0\n\t"
        "pushl $0x15\n\t"
        "pushl %[height]\n\t"
        "pushl %[width]\n\t"
        "xorl %%ecx, %%ecx\n\t"
        "movl %[name], %%eax\n\t"
        "call *%[factory]\n\t"
        "addl $16, %%esp\n\t"
        "movl %%eax, %[wrapper]\n\t"
        "testl %%eax, %%eax\n\t"
        "jz 1f\n\t"
        "leal 8(%%eax), %%eax\n\t"
        "1:\n\t"
        "movl %%eax, %[surface]\n\t"
        : [wrapper] "=m" (wrapper),
          [surface] "=m" (surface)
        : [height] "r" (height),
          [width] "r" (width),
          [name] "r" (spirit_history_resource_name),
          [factory] "r" (native_history_resource_factory)
        : "eax", "ecx", "edx", "memory"
    );
    if (wrapper == NULL || !readable_memory(surface, 0x0cu)) {
        return NULL;
    }
    spirit_player_two_history_wrapper = wrapper;
    spirit_player_two_history_width = width;
    spirit_player_two_history_height = height;
    return surface;
}

static BOOL routed_camera_effect_active(void) {
    DWORD now = GetTickCount();
    BOOL native_skill_active =
        character_is_using_skill(player_one_character) ||
        character_is_using_skill(player_two_character);
    BOOL restore_tail_active = skill_camera_history_tail_until != 0u &&
        (LONG)(skill_camera_history_tail_until - now) > 0;

    if (skill_camera_history_tail_until != 0u && !restore_tail_active) {
        skill_camera_history_tail_until = 0u;
    }
    return current_spirit_presentation_state() != 0 ||
        native_skill_active ||
        player_skill_render_states[0] != NULL ||
        player_skill_render_states[1] != NULL ||
        restore_tail_active;
}

static BOOL isolate_camera_effect_from_player_two(void) {
    return !fixed_three_assignment_selected() &&
        spirit_strike_viewport_effect_isolation_enabled &&
        rendered_player_two_this_frame &&
        routed_camera_effect_active();
}

static BOOL ensure_camera_history_resource(void) {
    uint8_t *effect = current_spirit_motion_blur_effect();

    if (!spirit_strike_viewport_effect_isolation_enabled || effect == NULL ||
        !readable_memory(effect + 0x10u, sizeof(void *))) {
        return FALSE;
    }
    if (spirit_player_two_history_surface != NULL &&
        spirit_player_two_history_width == frame_cache_description.width &&
        spirit_player_two_history_height == frame_cache_description.height) {
        return TRUE;
    }
    if (spirit_player_two_history_surface != NULL) {
        return FALSE;
    }
    spirit_player_two_history_surface = create_spirit_history_surface();
    if (spirit_player_two_history_surface == NULL) {
        if (!spirit_history_resource_logged) {
            spirit_history_resource_logged = TRUE;
            SudekiMpLogWrite(
                "split_screen_render event=spirit_history_resource phase=create_failed policy=native_callbacks_unchanged\r\n"
            );
        }
        return FALSE;
    }
    SudekiMpLogFormat(
        "split_screen_render event=spirit_history_resource phase=created wrapper=0x%08lx surface=0x%08lx dimensions=%ux%u policy=process_lifetime_native_factory\r\n",
        (unsigned long)(uintptr_t)spirit_player_two_history_wrapper,
        (unsigned long)(uintptr_t)spirit_player_two_history_surface,
        spirit_player_two_history_width,
        spirit_player_two_history_height
    );
    return TRUE;
}

static void log_spirit_effect_isolation_once(void) {
    if (spirit_effect_isolation_logged) {
        return;
    }
    spirit_effect_isolation_logged = TRUE;
    SudekiMpLogFormat(
        "split_screen_render event=camera_effect_viewport_isolation "
        "phase=active routed_player_one=%u routed_player_two=%u spirit_state=%d "
        "player_one_history=native "
        "player_two_history=viewport_owned "
        "callbacks=native_unmodified "
        "player_two_render_and_input=live\r\n",
        player_skill_render_states[0] != NULL ? 1u : 0u,
        player_skill_render_states[1] != NULL ? 1u : 0u,
        current_spirit_presentation_state()
    );
}

static BOOL swap_spirit_history_pointer(
    void **slot,
    void **saved
) {
    if (!isolate_camera_effect_from_player_two() ||
        spirit_player_two_history_surface == NULL || slot == NULL ||
        saved == NULL || !readable_memory(slot, sizeof(*slot))) {
        return FALSE;
    }
    *saved = *slot;
    if (*saved == spirit_player_two_history_surface) {
        return FALSE;
    }
    *slot = spirit_player_two_history_surface;
    log_spirit_effect_isolation_once();
    return TRUE;
}

static void restore_spirit_history_pointer(
    void **slot,
    void *saved,
    BOOL swapped
) {
    if (swapped && slot != NULL && readable_memory(slot, sizeof(*slot))) {
        *slot = saved;
    }
}

static void SUDEKIMP_THISCALL route_motion_blur_post_render(
    void *callback,
    unsigned char flags
) {
    void **history_slot = NULL;
    void *saved_history = NULL;
    void *active_history = NULL;
    BOOL swapped = FALSE;
    unsigned int trace_bit = rendered_player_two_this_frame ? 2u : 1u;

    if (fixed_three_assignment_selected()) {
        original_motion_blur_post_render(callback, flags);
        return;
    }

    if (isolate_camera_effect_from_player_two() &&
        readable_memory(callback, 0x14u)) {
        history_slot = (void **)((uint8_t *)callback + 0x10u);
        swapped = swap_spirit_history_pointer(
            history_slot,
            &saved_history
        );
    }
    if (readable_memory(callback, 0x14u)) {
        active_history = *(void **)((uint8_t *)callback + 0x10u);
    }
    if (skill_camera_trace_active &&
        (skill_camera_history_logged_callbacks & trace_bit) == 0u) {
        skill_camera_history_logged_callbacks |= trace_bit;
        SudekiMpLogFormat(
            "skill_camera_trace event=post_effect_callback sequence=%u kind=motion_blur viewport=player_%u callback=0x%08lx history=0x%08lx swapped=%u p1_using_skill=%u p2_using_skill=%u routed_p1=%u routed_p2=%u spirit_state=%d policy=native_callback_unmodified_private_player_two_history_for_full_native_skill_lifetime\r\n",
            skill_camera_trace_sequence,
            rendered_player_two_this_frame ? 2u : 1u,
            (unsigned long)(uintptr_t)callback,
            (unsigned long)(uintptr_t)active_history,
            swapped ? 1u : 0u,
            character_is_using_skill(player_one_character) ? 1u : 0u,
            character_is_using_skill(player_two_character) ? 1u : 0u,
            player_skill_render_states[0] != NULL ? 1u : 0u,
            player_skill_render_states[1] != NULL ? 1u : 0u,
            current_spirit_presentation_state()
        );
    }
    original_motion_blur_post_render(callback, flags);
    restore_spirit_history_pointer(history_slot, saved_history, swapped);
}

static void SUDEKIMP_THISCALL route_screenshot_post_render(
    void *callback,
    unsigned char flags
) {
    BOOL rendered_player_two = rendered_player_two_this_frame;
    unsigned int completion_before = 0u;
    unsigned int completion_after = 0u;
    void **history_slot = NULL;
    void *saved_history = NULL;
    void *active_history = NULL;
    BOOL swapped = FALSE;
    unsigned int trace_bit = rendered_player_two ? 8u : 4u;

    if (fixed_three_assignment_selected()) {
        original_screenshot_post_render(callback, flags);
        return;
    }

    if (readable_memory(callback, 0x0cu)) {
        completion_before = ((uint8_t *)callback)[0x08u];
    }
    if (isolate_camera_effect_from_player_two() &&
        readable_memory(callback, 0x08u)) {
        history_slot = (void **)((uint8_t *)callback + 0x04u);
        swapped = swap_spirit_history_pointer(
            history_slot,
            &saved_history
        );
    }
    if (readable_memory(callback, 0x08u)) {
        active_history = *(void **)((uint8_t *)callback + 0x04u);
    }
    if (skill_camera_trace_active &&
        (skill_camera_history_logged_callbacks & trace_bit) == 0u) {
        skill_camera_history_logged_callbacks |= trace_bit;
        SudekiMpLogFormat(
            "skill_camera_trace event=post_effect_callback sequence=%u kind=screenshot viewport=player_%u callback=0x%08lx history=0x%08lx swapped=%u p1_using_skill=%u p2_using_skill=%u routed_p1=%u routed_p2=%u spirit_state=%d policy=native_callback_unmodified_private_player_two_history_for_full_native_skill_lifetime\r\n",
            skill_camera_trace_sequence,
            rendered_player_two ? 2u : 1u,
            (unsigned long)(uintptr_t)callback,
            (unsigned long)(uintptr_t)active_history,
            swapped ? 1u : 0u,
            character_is_using_skill(player_one_character) ? 1u : 0u,
            character_is_using_skill(player_two_character) ? 1u : 0u,
            player_skill_render_states[0] != NULL ? 1u : 0u,
            player_skill_render_states[1] != NULL ? 1u : 0u,
            current_spirit_presentation_state()
        );
    }
    original_screenshot_post_render(callback, flags);
    restore_spirit_history_pointer(history_slot, saved_history, swapped);
    if (readable_memory(callback, 0x0cu)) {
        completion_after = ((uint8_t *)callback)[0x08u];
    }
    if (spirit_strike_viewport_effect_isolation_enabled &&
        current_spirit_presentation_state() != 0 &&
        !spirit_capture_completion_logged) {
        spirit_capture_completion_logged = TRUE;
        SudekiMpLogFormat(
            "split_screen_render event=spirit_history_capture "
            "phase=completed render_view=player_%u "
            "completion_before=%u completion_after=%u "
            "policy=native_callback_never_suppressed\r\n",
            rendered_player_two ? 2u : 1u,
            completion_before,
            completion_after
        );
    }
}

static void trace_spirit_presentation(BOOL rendered_player_two) {
    uint8_t *manager;
    uint8_t *character;
    uint8_t *position;
    uint8_t *component;
    void *attached_model;
    void *first_person_wrapper;
    void *saved_world_wrapper;
    void *first_person_render_object;
    void *saved_world_render_object;
    void *native_camera;
    void *native_render_state;
    void **scene_render_slot;
    uint32_t first_person_flags = 0u;
    uint32_t saved_world_flags = 0u;
    int state;
    unsigned int view_bit;

    if (game_base == NULL || !readable_memory(
            game_base + RVA_SPIRIT_STRIKE_MANAGER_GLOBAL,
            sizeof(manager))) {
        return;
    }
    manager = *(uint8_t **)(game_base + RVA_SPIRIT_STRIKE_MANAGER_GLOBAL);
    state = current_spirit_presentation_state();
    if (state != spirit_presentation_last_state) {
        spirit_presentation_last_state = state;
        spirit_presentation_logged_views = 0u;
        spirit_effect_isolation_logged = FALSE;
        SudekiMpLogFormat(
            "split_screen_render event=spirit_presentation phase=%s manager=0x%08lx state=%d policy=observation_only\r\n",
            state == 0 ? "inactive" : "active",
            (unsigned long)(uintptr_t)manager,
            state
        );
        if (state == 0) {
            spirit_capture_completion_logged = FALSE;
            SudekiMpSplitScreenClearSkillCamera(
                player_one_character,
                "spirit_presentation_inactive"
            );
        }
    }
    if (state == 0 || player_one_character == NULL) {
        return;
    }
    view_bit = rendered_player_two ? 2u : 1u;
    if ((spirit_presentation_logged_views & view_bit) != 0u) {
        return;
    }
    spirit_presentation_logged_views |= view_bit;
    character = (uint8_t *)player_one_character;
    position = readable_memory(character, 0x98u) ?
        *(uint8_t **)(character + 0x44u) : NULL;
    component = readable_memory(character, 0x138u) ?
        *(uint8_t **)(character + 0x134u) : NULL;
    attached_model = readable_memory(position, 0xb8u) ?
        *(void **)(position + 0xb4u) : NULL;
    first_person_wrapper = readable_memory(component, 0x168u) ?
        *(void **)(component + 0x160u) : NULL;
    saved_world_wrapper = readable_memory(component, 0x168u) ?
        *(void **)(component + 0x164u) : NULL;
    first_person_render_object = readable_memory(
            first_person_wrapper, 0x0cu) ?
        *(void **)((uint8_t *)first_person_wrapper + 0x08u) : NULL;
    saved_world_render_object = readable_memory(saved_world_wrapper, 0x0cu) ?
        *(void **)((uint8_t *)saved_world_wrapper + 0x08u) : NULL;
    if (readable_memory(first_person_render_object, 0x38u)) {
        first_person_flags = *(uint32_t *)(
            (uint8_t *)first_person_render_object + 0x34u
        );
    }
    if (readable_memory(saved_world_render_object, 0x38u)) {
        saved_world_flags = *(uint32_t *)(
            (uint8_t *)saved_world_render_object + 0x34u
        );
    }
    native_camera = current_render_camera(second_player_camera_manager);
    native_render_state = readable_memory(native_camera, 0x38u) ?
        *(void **)((uint8_t *)native_camera + 0x34u) : NULL;
    scene_render_slot = current_scene_render_camera_slot();
    SudekiMpLogFormat(
        "split_screen_render event=spirit_presentation phase=model_snapshot viewport=%u native_camera=0x%08lx native_render_state=0x%08lx scene_render_state=0x%08lx player_two_render_state=0x%08lx character=0x%08lx component=0x%08lx attached_model=0x%08lx first_person_wrapper=0x%08lx first_person_render_object=0x%08lx first_person_flags=0x%08lx saved_world_wrapper=0x%08lx saved_world_render_object=0x%08lx saved_world_flags=0x%08lx policy=observation_only\r\n",
        rendered_player_two ? 2u : 1u,
        (unsigned long)(uintptr_t)native_camera,
        (unsigned long)(uintptr_t)native_render_state,
        (unsigned long)(uintptr_t)(scene_render_slot == NULL ?
            NULL : *scene_render_slot),
        (unsigned long)(uintptr_t)player_two_render_state,
        (unsigned long)(uintptr_t)character,
        (unsigned long)(uintptr_t)component,
        (unsigned long)(uintptr_t)attached_model,
        (unsigned long)(uintptr_t)first_person_wrapper,
        (unsigned long)(uintptr_t)first_person_render_object,
        (unsigned long)first_person_flags,
        (unsigned long)(uintptr_t)saved_world_wrapper,
        (unsigned long)(uintptr_t)saved_world_render_object,
        (unsigned long)saved_world_flags
    );
}

void *SudekiMpSplitScreenHudPartySourceDispatch(void *source) {
    uint8_t *group;
    uint8_t *player_one_source;
    uint8_t *player_two_source;
    void *desired_character;
    void *resolved_source;
    unsigned int index;
    unsigned int source_role;
    unsigned int viewport_index;
    unsigned int resolved_slot = UINT_MAX;
    unsigned int expected_slot = UINT_MAX;
    unsigned int matches = 0u;

    if (fixed_three_runtime.transaction_active &&
        fixed_three_runtime.presentation_clear_this_frame &&
        viewport_hud_binding_active &&
        fixed_three_runtime.rendered_seat < FIXED_THREE_SEAT_COUNT) {
        unsigned int owner_seat = fixed_three_runtime.rendered_seat;
        unsigned int owner_slot =
            fixed_three_runtime.party_slots[owner_seat];

        if (game_base == NULL || !readable_memory(
                game_base + RVA_ACTIVE_GROUP_GLOBAL, sizeof(group))) {
            fixed_three_runtime.hud_source_failure_this_frame = TRUE;
            return source;
        }
        group = *(uint8_t **)(game_base + RVA_ACTIVE_GROUP_GLOBAL);
        if (group != fixed_three_runtime.group ||
            owner_slot >= PARTY_SLOT_COUNT ||
            !readable_memory(group + PARTY_SLOT_ZERO_OFFSET,
                PARTY_SLOT_COUNT * PARTY_SLOT_STRIDE)) {
            fixed_three_runtime.hud_source_failure_this_frame = TRUE;
            return source;
        }
        player_one_source = group + PARTY_SLOT_ZERO_OFFSET;
        player_two_source = player_one_source +
            owner_slot * PARTY_SLOT_STRIDE;
        if (source == player_one_source) {
            source_role = 0u;
            desired_character = fixed_three_runtime.actors[owner_seat];
            expected_slot = owner_slot;
        } else if (owner_seat != 0u && source == player_two_source) {
            source_role = 1u;
            desired_character = fixed_three_runtime.actors[0];
            expected_slot = 0u;
        } else {
            /* Native HUD paths for untouched AI slots retain their own source
             * and do not constitute evidence for or against the rendered
             * human seat. */
            return source;
        }
        resolved_source = NULL;
        for (index = 0u; index < PARTY_SLOT_COUNT; ++index) {
            uint8_t *candidate_source = player_one_source +
                index * PARTY_SLOT_STRIDE;

            if (*(void **)candidate_source == desired_character) {
                resolved_source = candidate_source;
                resolved_slot = index;
                ++matches;
            }
        }
        if (matches != 1u || resolved_slot != expected_slot ||
            resolved_source != player_one_source +
                expected_slot * PARTY_SLOT_STRIDE) {
            fixed_three_runtime.hud_source_failure_this_frame = TRUE;
            return source;
        }
        if (fixed_three_runtime.hud_role_mask != 0u &&
            fixed_three_runtime.hud_evidence_seat != owner_seat) {
            fixed_three_runtime.hud_source_failure_this_frame = TRUE;
            return source;
        }
        fixed_three_runtime.hud_evidence_seat = owner_seat;
        fixed_three_runtime.hud_role_mask |= 1u << source_role;
        viewport_index = owner_seat;
        if (!hud_mapping_trace_valid[viewport_index][source_role] ||
            hud_mapping_trace_resolved_slot[viewport_index][source_role] !=
                resolved_slot) {
            hud_mapping_trace_valid[viewport_index][source_role] = TRUE;
            hud_mapping_trace_resolved_slot[viewport_index][source_role] =
                resolved_slot;
            SudekiMpLogFormat(
                "split_screen_render event=fixed_three_hud_mapping "
                "viewport=%u source_role=%s resolved_slot=%ld "
                "policy=stable_seat_actor_swap_no_party_write\r\n",
                viewport_index + 1u,
                source_role == 0u ? "primary" : "owner_slot",
                resolved_slot == UINT_MAX ? -1L : (long)resolved_slot);
        }
        return resolved_source;
    }

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
    if (group == NULL || !readable_memory(
            group + PARTY_SLOT_ZERO_OFFSET,
            PARTY_SLOT_COUNT * PARTY_SLOT_STRIDE)) {
        return source;
    }
    player_one_source = group + PARTY_SLOT_ZERO_OFFSET;
    player_two_source = player_one_source +
        player_two_party_slot * PARTY_SLOT_STRIDE;
    if (source == player_one_source) {
        source_role = 0u;
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
        source_role = 1u;
        desired_character = rendered_player_two_this_frame ?
            player_one_character : player_two_character;
    } else {
        return source;
    }
    resolved_source = source;
    for (index = 0u; index < PARTY_SLOT_COUNT; ++index) {
        uint8_t *candidate_source = player_one_source +
            index * PARTY_SLOT_STRIDE;
        if (*(void **)candidate_source == desired_character) {
            resolved_source = candidate_source;
            resolved_slot = index;
            break;
        }
    }
    viewport_index = rendered_player_two_this_frame ? 1u : 0u;
    if (!hud_mapping_trace_valid[viewport_index][source_role] ||
        hud_mapping_trace_resolved_slot[viewport_index][source_role] !=
            resolved_slot) {
        hud_mapping_trace_valid[viewport_index][source_role] = TRUE;
        hud_mapping_trace_resolved_slot[viewport_index][source_role] =
            resolved_slot;
        SudekiMpLogFormat(
            "split_screen_render event=hud_party_mapping viewport=%u source_role=%s desired_character=0x%08lx resolved_slot=%ld player_one_character=0x%08lx player_two_character=0x%08lx player_two_party_slot=%u policy=observation_of_native_hud_copy_source\r\n",
            viewport_index + 1u,
            source_role == 0u ? "primary" : "companion",
            (unsigned long)(uintptr_t)desired_character,
            resolved_slot == UINT_MAX ? -1L : (long)resolved_slot,
            (unsigned long)(uintptr_t)player_one_character,
            (unsigned long)(uintptr_t)player_two_character,
            player_two_party_slot
        );
    }
    return resolved_source;
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

static void *minimap_party_source_dispatch(
    void *source,
    void *desired_character,
    unsigned int owner_seat,
    BOOL *resolved,
    const char *phase
) {
    uint8_t *group;
    uint8_t *party_source;
    unsigned int index;
    unsigned int viewport_index;

    if (resolved != NULL) {
        *resolved = FALSE;
    }
    if (game_base == NULL || desired_character == NULL ||
        !readable_memory(
            game_base + RVA_ACTIVE_GROUP_GLOBAL,
            sizeof(group))) {
        minimap_source_failure_this_frame = TRUE;
        return source;
    }
    group = *(uint8_t **)(game_base + RVA_ACTIVE_GROUP_GLOBAL);
    if (group == NULL || !readable_memory(
            group + PARTY_SLOT_ZERO_OFFSET,
            PARTY_SLOT_COUNT * PARTY_SLOT_STRIDE)) {
        minimap_source_failure_this_frame = TRUE;
        return source;
    }
    party_source = group + PARTY_SLOT_ZERO_OFFSET;
    if (source != party_source) {
        minimap_source_failure_this_frame = TRUE;
        return source;
    }
    for (index = 0u; index < PARTY_SLOT_COUNT; ++index) {
        uint8_t *candidate_source = party_source +
            index * PARTY_SLOT_STRIDE;
        if (*(void **)candidate_source == desired_character) {
            viewport_index = owner_seat < FIXED_THREE_SEAT_COUNT ?
                owner_seat : 0u;
            if (!minimap_ownership_logged[viewport_index]) {
                minimap_ownership_logged[viewport_index] = TRUE;
                SudekiMpLogFormat(
                    "split_screen_render event=minimap_ownership phase=%s viewport=%u desired_character=0x%08lx resolved_slot=%u policy=stable_character_identity_per_cached_view_no_party_or_controller_write\r\n",
                    phase,
                    viewport_index + 1u,
                    (unsigned long)(uintptr_t)desired_character,
                    index
                );
            }
            if (resolved != NULL) {
                *resolved = TRUE;
            }
            return candidate_source;
        }
    }
    minimap_source_failure_this_frame = TRUE;
    return source;
}

void *SudekiMpSplitScreenMinimapScheduledPartySourceDispatch(void *source) {
    BOOL resolved = FALSE;
    void *desired_character;
    void *resolved_source;

    minimap_update_owner_valid = FALSE;
    minimap_update_player_two = FALSE;
    minimap_update_character = NULL;
    if (fixed_three_assignment_selected()) {
        unsigned int owner_seat =
            fixed_three_runtime.minimap_expected_update_seat;

        if (fixed_three_runtime.transaction_active ||
            !fixed_three_runtime.configured ||
            !fixed_three_runtime.cameras_acquired ||
            !fixed_three_base_leases_exact() ||
            owner_seat >= FIXED_THREE_SEAT_COUNT ||
            fixed_three_runtime.minimap_expected_update_epoch == 0u ||
            inspect_shared_interaction_modal(NULL) !=
                SUDEKIMP_SHARED_INTERACTION_MODAL_NONE ||
            pc_quit_screen_visible() ||
            quick_menu_visible() ||
            genuine_quick_menu_visible() ||
            current_spirit_presentation_state() != 0 ||
            settled_temporary_zone_active() ||
            coop_roster_party_transition_active) {
            /* UIMapManager::Update runs before RenderStart.  A cache frame
             * scheduled by the previous clear presentation must not remap a
             * native full-width modal/special frame that became active in
             * between.  Preserve the arm but withhold proof; RenderStart and
             * FrameEnd will invalidate it and require a fresh clear retry. */
            fixed_three_runtime.minimap_update_valid = FALSE;
            fixed_three_runtime.minimap_update_seat =
                SUDEKIMP_LOCAL_VIEWPORT_NO_CONTROLLER;
            fixed_three_runtime.minimap_update_epoch = 0u;
            return source;
        }
        fixed_three_runtime.minimap_update_valid = FALSE;
        fixed_three_runtime.minimap_update_seat =
            SUDEKIMP_LOCAL_VIEWPORT_NO_CONTROLLER;
        fixed_three_runtime.minimap_update_epoch = 0u;
        desired_character = fixed_three_runtime.actors[owner_seat];
        resolved_source = minimap_party_source_dispatch(
            source,
            desired_character,
            owner_seat,
            &resolved,
            "fixed_three_update");
        if (resolved) {
            fixed_three_runtime.minimap_update_valid = TRUE;
            fixed_three_runtime.minimap_update_seat = (uint8_t)owner_seat;
            fixed_three_runtime.minimap_update_epoch =
                fixed_three_runtime.minimap_expected_update_epoch;
        }
        return resolved_source;
    }
    if (!dual_camera_frame_cache_enabled ||
        !minimap_scheduled_owner_valid) {
        return source;
    }
    minimap_update_player_two =
        SudekiMpSplitScreenMinimapOwnerIsPlayerTwo(
            dual_camera_frame_cache_enabled,
            minimap_scheduled_owner_valid,
            minimap_scheduled_player_two
        );
    desired_character = minimap_update_player_two ?
        player_two_character : player_one_character;
    resolved_source = minimap_party_source_dispatch(
        source,
        desired_character,
        minimap_update_player_two ? 1u : 0u,
        &resolved,
        "update"
    );
    if (resolved) {
        minimap_update_owner_valid = TRUE;
        minimap_update_character = desired_character;
    }
    return resolved_source;
}

void *SudekiMpSplitScreenMinimapRenderPartySourceDispatch(void *source) {
    void *expected_character;
    void *resolved_source;
    BOOL resolved = FALSE;

    if (fixed_three_runtime.transaction_active &&
        fixed_three_runtime.presentation_clear_this_frame) {
        unsigned int owner_seat =
            fixed_three_runtime.minimap_update_seat;

        if (!fixed_three_runtime.minimap_update_valid ||
            owner_seat >= FIXED_THREE_SEAT_COUNT ||
            owner_seat != fixed_three_runtime.rendered_seat ||
            owner_seat !=
                fixed_three_runtime.minimap_expected_update_seat ||
            fixed_three_runtime.minimap_update_epoch == 0u ||
            fixed_three_runtime.minimap_update_epoch !=
                fixed_three_runtime.minimap_expected_update_epoch) {
            minimap_source_failure_this_frame = TRUE;
            return source;
        }
        resolved_source = minimap_party_source_dispatch(
            source,
            fixed_three_runtime.actors[owner_seat],
            owner_seat,
            &resolved,
            "fixed_three_render");
        if (resolved) {
            fixed_three_runtime.minimap_render_valid = TRUE;
            fixed_three_runtime.minimap_render_seat = (uint8_t)owner_seat;
        }
        return resolved_source;
    }

    if (!dual_camera_frame_cache_enabled) {
        return source;
    }
    expected_character = minimap_update_player_two ?
        player_two_character : player_one_character;
    if (!minimap_update_owner_valid ||
        minimap_update_character == NULL ||
        minimap_update_character != expected_character) {
        minimap_source_failure_this_frame = TRUE;
        return source;
    }
    return minimap_party_source_dispatch(
        source,
        minimap_update_character,
        minimap_update_player_two ? 1u : 0u,
        NULL,
        "render"
    );
}

__attribute__((naked, noinline, used))
static void split_screen_minimap_scheduled_party_pointer_copy_entry(void) {
    __asm__ volatile(
        "pushl %eax\n\t"
        "pushl %ecx\n\t"
        "call _SudekiMpSplitScreenMinimapScheduledPartySourceDispatch\n\t"
        "addl $4, %esp\n\t"
        "movl %eax, %ecx\n\t"
        "popl %eax\n\t"
        "call *_original_hud_party_pointer_copy\n\t"
        "ret\n\t"
    );
}

__attribute__((naked, noinline, used))
static void split_screen_minimap_render_party_pointer_copy_entry(void) {
    __asm__ volatile(
        "pushl %eax\n\t"
        "pushl %ecx\n\t"
        "call _SudekiMpSplitScreenMinimapRenderPartySourceDispatch\n\t"
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

static unsigned int refresh_viewport_portraits(void) {
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
    unsigned int current_portrait_enum;
    unsigned int portrait_enum;
    unsigned int resource_index;
    unsigned int assigned_count = 0u;
    unsigned int proven_role_mask = 0u;
    BOOL fixed_three_view = fixed_three_runtime.transaction_active &&
        fixed_three_runtime.presentation_clear_this_frame &&
        fixed_three_runtime.rendered_seat < FIXED_THREE_SEAT_COUNT;
    unsigned int fixed_three_owner_seat =
        fixed_three_runtime.rendered_seat;
    unsigned int fixed_three_owner_slot = fixed_three_owner_seat <
            FIXED_THREE_SEAT_COUNT ?
        fixed_three_runtime.party_slots[fixed_three_owner_seat] :
        PARTY_SLOT_COUNT;

    if (!dual_camera_frame_cache_enabled || game_base == NULL ||
        original_character_type_to_portrait_enum == NULL ||
        original_hud_portrait_resource_select == NULL ||
        shared_interaction_modal_observation !=
            SUDEKIMP_SHARED_INTERACTION_MODAL_NONE ||
        InterlockedCompareExchange(
            &native_save_modal_opening,
            0,
            0) != 0 ||
        !readable_memory(
            game_base + RVA_GAMEPLAY_HUD_GLOBAL,
            sizeof(hud)) ||
        !readable_memory(
            game_base + RVA_ACTIVE_GROUP_GLOBAL,
            sizeof(group))) {
        return 0u;
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
        return 0u;
    }
    group_size = *(unsigned int *)(group + 0xccu);
    if (group_size > PARTY_SLOT_COUNT) {
        group_size = PARTY_SLOT_COUNT;
    }
    expected_vtable = game_base + RVA_HUD_PORTRAIT_GIZMO_VTABLE;
    for (index = 0u; index < group_size; ++index) {
        unsigned int fixed_three_role_bit = 0u;

        if (fixed_three_view && index == 0u) {
            fixed_three_role_bit = 0x01u;
        } else if (fixed_three_view && fixed_three_owner_seat != 0u &&
                   index == fixed_three_owner_slot) {
            fixed_three_role_bit = 0x02u;
        }
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
        if (fixed_three_view && index == 0u) {
            character = (uint8_t *)
                fixed_three_runtime.actors[fixed_three_owner_seat];
        } else if (fixed_three_view && fixed_three_owner_seat != 0u &&
                   index == fixed_three_owner_slot) {
            character = (uint8_t *)fixed_three_runtime.actors[0];
        } else if (index == 0u) {
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
                sizeof(resource_index)) ||
            !writable_memory(
                (uint8_t *)portrait_gizmo + HUD_PORTRAIT_ENUM_OFFSET,
                sizeof(current_portrait_enum))) {
            continue;
        }
        current_portrait_enum = *(unsigned int *)(
            (uint8_t *)portrait_gizmo + HUD_PORTRAIT_ENUM_OFFSET
        );
        if (current_portrait_enum != portrait_enum) {
            if (!SudekiMpSplitScreenViewportPortraitAssignmentNeeded(
                    shared_interaction_modal_observation !=
                        SUDEKIMP_SHARED_INTERACTION_MODAL_NONE ||
                    InterlockedCompareExchange(
                        &native_save_modal_opening,
                        0,
                        0) != 0,
                    current_portrait_enum,
                    portrait_enum)) {
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
            ++assigned_count;
        }
        if (*(unsigned int *)((uint8_t *)portrait_gizmo +
                HUD_PORTRAIT_ENUM_OFFSET) == portrait_enum) {
            proven_role_mask |= fixed_three_role_bit;
        }
    }
    if (!viewport_portrait_ownership_logged &&
        (rendered_player_two_this_frame || fixed_three_view) &&
        assigned_count != 0u) {
        viewport_portrait_ownership_logged = TRUE;
        SudekiMpLogFormat(
            "split_screen_render event=viewport_hud_portrait phase=active "
            "viewport=%u assigned_gizmos=%u owner_party_slot=%u "
            "policy=skip_redundant_same_owner_assignment_call_on_viewport_enum_change\r\n",
            fixed_three_view ? fixed_three_owner_seat + 1u : 2u,
            assigned_count,
            fixed_three_view ? fixed_three_owner_slot :
                player_two_party_slot
        );
    }
    return fixed_three_view ? proven_role_mask : 0u;
}

static BOOL resolve_player_characters(
    void **first_character,
    void **second_character,
    unsigned int *second_slot
) {
    uint8_t *group;
    uint8_t *controller;
    void *controller_target;
    SudekiMpPlayerCombatSnapshot second_player_context;
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
    if (coop_roster_valid) {
        if (!character_has_resource_type(
                controller_target,
                coop_roster_player_one_type)) {
            return FALSE;
        }
        for (index = 1u; index < PARTY_SLOT_COUNT; ++index) {
            void *candidate = *(void **)(
                group + PARTY_SLOT_ZERO_OFFSET + index * PARTY_SLOT_STRIDE
            );
            if (candidate != NULL &&
                character_has_resource_type(
                    candidate,
                    coop_roster_player_two_type)) {
                *first_character = controller_target;
                *second_character = candidate;
                *second_slot = index;
                return TRUE;
            }
        }
        return FALSE;
    }
    if (coop_role_lock_active) {
        if (coop_locked_player_one == NULL ||
            coop_locked_player_two == NULL ||
            controller_target != coop_locked_player_one) {
            return FALSE;
        }
        for (index = 1u; index < PARTY_SLOT_COUNT; ++index) {
            void *candidate = *(void **)(
                group + PARTY_SLOT_ZERO_OFFSET + index * PARTY_SLOT_STRIDE
            );
            if (candidate == coop_locked_player_two) {
                *first_character = coop_locked_player_one;
                *second_character = coop_locked_player_two;
                *second_slot = index;
                return TRUE;
            }
        }
        return FALSE;
    }
    if (SudekiMpCombatContextGetSnapshot(
            1u,
            &second_player_context
        ) && second_player_context.character != NULL &&
        second_player_context.character != controller_target) {
        if (!SudekiMpSplitScreenExternalPlayerTwoLeasePolicy(
                runtime_authorization_query != NULL,
                SudekiMpControlSeparationPlayerTwoRequested(),
                SudekiMpControlSeparationPlayerTwoActive(),
                second_player_context.character,
                SudekiMpControlSeparationPlayerTwoCharacter())) {
            return FALSE;
        }
        for (index = 1u; index < PARTY_SLOT_COUNT; ++index) {
            void *candidate = *(void **)(
                group + PARTY_SLOT_ZERO_OFFSET + index * PARTY_SLOT_STRIDE
            );
            if (candidate == second_player_context.character) {
                *first_character = controller_target;
                *second_character = candidate;
                *second_slot = index;
                return TRUE;
            }
        }
        return FALSE;
    }
    /* A closed externally-authorized profile must name Player 2 through the
     * already-proven control/combat lease.  Falling back to the first
     * non-front party slot could bind an AI companion before that lease is
     * published or after it is revoked. */
    if (runtime_authorization_query != NULL) {
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

typedef struct SudekiMpSkillCameraTraceMatrix {
    BOOL valid;
    float forward[3];
    float up[3];
    float eye[3];
    float projection[3];
} SudekiMpSkillCameraTraceMatrix;

static void capture_skill_camera_trace_matrix(
    void *render_state,
    SudekiMpSkillCameraTraceMatrix *result
) {
    const float *matrix;
    const float *projection;
    unsigned int index;

    ZeroMemory(result, sizeof(*result));
    if (!readable_memory(render_state, 0xdcu)) {
        return;
    }
    matrix = (const float *)((const uint8_t *)render_state + 0x90u);
    projection = (const float *)((const uint8_t *)render_state + 0xd0u);
    for (index = 0u; index < 3u; ++index) {
        result->forward[index] = matrix[8u + index];
        result->up[index] = matrix[4u + index];
        result->eye[index] = matrix[12u + index];
        result->projection[index] = projection[index];
        if (!isfinite(result->forward[index]) ||
            !isfinite(result->up[index]) ||
            !isfinite(result->eye[index]) ||
            !isfinite(result->projection[index])) {
            ZeroMemory(result, sizeof(*result));
            return;
        }
    }
    result->valid = TRUE;
}

static void trace_skill_camera_frame(void) {
    BOOL player_one_using_skill =
        character_is_using_skill(player_one_character);
    BOOL player_two_using_skill =
        character_is_using_skill(player_two_character);
    BOOL active = player_one_using_skill || player_two_using_skill ||
        player_skill_render_states[0] != NULL ||
        player_skill_render_states[1] != NULL;
    DWORD now = GetTickCount();
    void *manager;
    void *native_camera;
    void *native_state = NULL;
    void **scene_slot;
    void *scene_state = NULL;
    void *player_one_state;
    void *player_two_state;
    SudekiMpSkillCameraTraceMatrix native_matrix;
    SudekiMpSkillCameraTraceMatrix scene_matrix;
    SudekiMpSkillCameraTraceMatrix player_one_matrix;
    SudekiMpSkillCameraTraceMatrix player_two_matrix;

    if (!active) {
        if (skill_camera_trace_active) {
            SudekiMpLogFormat(
                "skill_camera_trace event=lifecycle sequence=%u phase=end frames=%u elapsed_ms=%lu reason=native_skill_flags_and_routed_camera_states_clear policy=observation_only\r\n",
                skill_camera_trace_sequence,
                skill_camera_trace_frame,
                (unsigned long)(DWORD)(
                    now - skill_camera_trace_started_tick
                )
            );
        }
        skill_camera_trace_active = FALSE;
        skill_camera_trace_frame = 0u;
        skill_camera_trace_started_tick = 0u;
        return;
    }
    if (!skill_camera_trace_active) {
        skill_camera_trace_active = TRUE;
        ++skill_camera_trace_sequence;
        skill_camera_trace_frame = 0u;
        skill_camera_trace_started_tick = now;
        skill_camera_history_logged_callbacks = 0u;
        SudekiMpLogFormat(
            "skill_camera_trace event=lifecycle sequence=%u phase=begin p1_character=0x%08lx p2_character=0x%08lx p1_using_skill=%u p2_using_skill=%u p1_routed_state=0x%08lx p2_routed_state=0x%08lx policy=observation_only\r\n",
            skill_camera_trace_sequence,
            (unsigned long)(uintptr_t)player_one_character,
            (unsigned long)(uintptr_t)player_two_character,
            player_one_using_skill ? 1u : 0u,
            player_two_using_skill ? 1u : 0u,
            (unsigned long)(uintptr_t)player_skill_render_states[0],
            (unsigned long)(uintptr_t)player_skill_render_states[1]
        );
    }
    ++skill_camera_trace_frame;
    manager = current_camera_manager();
    native_camera = current_render_camera(manager);
    if (readable_memory(native_camera, 0x38u)) {
        native_state = *(void **)((uint8_t *)native_camera + 0x34u);
    }
    scene_slot = current_scene_render_camera_slot();
    if (readable_memory(scene_slot, sizeof(*scene_slot))) {
        scene_state = *scene_slot;
    }
    player_one_state = player_skill_render_states[0] != NULL ?
        player_skill_render_states[0] : player_one_render_state;
    player_two_state = player_skill_render_states[1] != NULL ?
        player_skill_render_states[1] : player_two_render_state;
    capture_skill_camera_trace_matrix(native_state, &native_matrix);
    capture_skill_camera_trace_matrix(scene_state, &scene_matrix);
    capture_skill_camera_trace_matrix(player_one_state, &player_one_matrix);
    capture_skill_camera_trace_matrix(player_two_state, &player_two_matrix);
    SudekiMpLogFormat(
        "skill_camera_trace event=frame sequence=%u frame=%u elapsed_ms=%lu viewport=%s p1_using_skill=%u p2_using_skill=%u native_camera=0x%08lx native_state=0x%08lx native_valid=%u native_forward=%.6f,%.6f,%.6f native_up=%.6f,%.6f,%.6f native_eye=%.6f,%.6f,%.6f native_projection=%.6f,%.6f,%.6f scene_state=0x%08lx scene_valid=%u scene_forward=%.6f,%.6f,%.6f scene_up=%.6f,%.6f,%.6f scene_eye=%.6f,%.6f,%.6f scene_projection=%.6f,%.6f,%.6f p1_state=0x%08lx p1_valid=%u p1_forward=%.6f,%.6f,%.6f p1_eye=%.6f,%.6f,%.6f p2_state=0x%08lx p2_valid=%u p2_forward=%.6f,%.6f,%.6f p2_eye=%.6f,%.6f,%.6f swap_active=%u swap_original=0x%08lx swap_applied=0x%08lx policy=read_only_every_rendered_skill_frame\r\n",
        skill_camera_trace_sequence,
        skill_camera_trace_frame,
        (unsigned long)(DWORD)(now - skill_camera_trace_started_tick),
        rendered_player_two_this_frame ? "player_two" : "player_one",
        player_one_using_skill ? 1u : 0u,
        player_two_using_skill ? 1u : 0u,
        (unsigned long)(uintptr_t)native_camera,
        (unsigned long)(uintptr_t)native_state,
        native_matrix.valid ? 1u : 0u,
        native_matrix.forward[0], native_matrix.forward[1],
        native_matrix.forward[2], native_matrix.up[0],
        native_matrix.up[1], native_matrix.up[2],
        native_matrix.eye[0], native_matrix.eye[1], native_matrix.eye[2],
        native_matrix.projection[0], native_matrix.projection[1],
        native_matrix.projection[2],
        (unsigned long)(uintptr_t)scene_state,
        scene_matrix.valid ? 1u : 0u,
        scene_matrix.forward[0], scene_matrix.forward[1],
        scene_matrix.forward[2], scene_matrix.up[0], scene_matrix.up[1],
        scene_matrix.up[2], scene_matrix.eye[0], scene_matrix.eye[1],
        scene_matrix.eye[2], scene_matrix.projection[0],
        scene_matrix.projection[1], scene_matrix.projection[2],
        (unsigned long)(uintptr_t)player_one_state,
        player_one_matrix.valid ? 1u : 0u,
        player_one_matrix.forward[0], player_one_matrix.forward[1],
        player_one_matrix.forward[2], player_one_matrix.eye[0],
        player_one_matrix.eye[1], player_one_matrix.eye[2],
        (unsigned long)(uintptr_t)player_two_state,
        player_two_matrix.valid ? 1u : 0u,
        player_two_matrix.forward[0], player_two_matrix.forward[1],
        player_two_matrix.forward[2], player_two_matrix.eye[0],
        player_two_matrix.eye[1], player_two_matrix.eye[2],
        render_only_swap_active ? 1u : 0u,
        (unsigned long)(uintptr_t)render_only_original_state,
        (unsigned long)(uintptr_t)render_only_applied_state
    );
}

static BOOL apply_player_two_controller_camera_input(
    float matrix[16],
    const float target[3],
    BOOL first_person
) {
    SudekiMpInputBridgeState input;
    DWORD now = GetTickCount();
    DWORD elapsed_ms;
    float raw_x;
    float raw_y;
    float magnitude;
    float direction_magnitude;
    float scaled_magnitude;
    float axis_x;
    float axis_y;
    float yaw_delta;
    float pitch_delta;
    float proposed_pitch;

    if (!second_player_controller_camera_enabled) {
        return TRUE;
    }
    if (SudekiMpControlSeparationGameplayInputFrozen()) {
        player_two_camera_input_last_tick = now;
        return TRUE;
    }
    elapsed_ms = player_two_camera_input_last_tick == 0u ? 0u :
        (DWORD)(now - player_two_camera_input_last_tick);
    player_two_camera_input_last_tick = now;
    if (elapsed_ms > 100u) {
        elapsed_ms = 100u;
    }
    if (!SudekiMpInputBridgePoll(&input) || elapsed_ms == 0u) {
        return TRUE;
    }
    raw_x = (float)input.right_x / 32768.0f;
    raw_y = -(float)input.right_y / 32768.0f;
    magnitude = sqrtf(raw_x * raw_x + raw_y * raw_y);
    if (magnitude <= second_player_controller_camera_deadzone) {
        return TRUE;
    }
    direction_magnitude = magnitude;
    if (magnitude > 1.0f) {
        magnitude = 1.0f;
    }
    scaled_magnitude =
        (magnitude - second_player_controller_camera_deadzone) /
        (1.0f - second_player_controller_camera_deadzone);
    axis_x = raw_x / direction_magnitude *
        scaled_magnitude;
    axis_y = raw_y / direction_magnitude *
        scaled_magnitude;
    yaw_delta = -axis_x * second_player_controller_camera_yaw_speed *
        ((float)elapsed_ms / 1000.0f);
    pitch_delta = axis_y * second_player_controller_camera_pitch_speed *
        ((float)elapsed_ms / 1000.0f);
    proposed_pitch = player_two_camera_pitch_offset + pitch_delta;
    if (proposed_pitch > second_player_controller_camera_maximum_pitch) {
        proposed_pitch = second_player_controller_camera_maximum_pitch;
    } else if (proposed_pitch <
               -second_player_controller_camera_maximum_pitch) {
        proposed_pitch = -second_player_controller_camera_maximum_pitch;
    }
    pitch_delta = proposed_pitch - player_two_camera_pitch_offset;
    if (first_person) {
        float eye[3] = {matrix[12], matrix[13], matrix[14]};

        if (!SudekiMpFirstPersonCameraTransform(
                matrix, eye, yaw_delta, pitch_delta) ||
            !SudekiMpOrbitCameraTransform(
                player_two_third_person_matrix,
                target,
                yaw_delta,
                pitch_delta)) {
            return FALSE;
        }
    } else if (!SudekiMpOrbitCameraTransform(
                   matrix, target, yaw_delta, pitch_delta)) {
        return FALSE;
    }
    player_two_camera_pitch_offset = proposed_pitch;
    if (!player_two_camera_input_logged) {
        player_two_camera_input_logged = TRUE;
        SudekiMpLogFormat(
            "split_screen_render event=player_two_controller_camera phase=active yaw_speed_bits=0x%08lx pitch_speed_bits=0x%08lx maximum_pitch_bits=0x%08lx deadzone_bits=0x%08lx policy=right_stick_render_only_camera_two_left_stick_uses_same_basis_supports_viewport_owned_first_person\r\n",
            (unsigned long)float_bits(second_player_controller_camera_yaw_speed),
            (unsigned long)float_bits(second_player_controller_camera_pitch_speed),
            (unsigned long)float_bits(second_player_controller_camera_maximum_pitch),
            (unsigned long)float_bits(second_player_controller_camera_deadzone)
        );
    }
    return TRUE;
}

/* Do not rotate Camera 2's render matrix after native Exploration has solved
 * collision: that would immediately put the eye back through the wall.  Feed
 * the controller's right stick into Camera 2's own action consumer instead.
 * The P1 broadcast remains filtered in route_camera_input_event(), so this
 * is the sole P2 input reaching the named camera. */
static BOOL apply_player_two_native_camera_input(void) {
    SudekiMpInputBridgeState input;
    SudekiMpNativeCameraInputEvent event;
    float raw_x;
    float raw_y;
    float magnitude;
    float scaled_magnitude;
    float axis_x;
    float axis_y;

    if (!second_player_controller_camera_enabled ||
        !native_second_player_camera_collision_session_enabled ||
        !player_two_native_camera_ready || player_two_camera == NULL ||
        original_camera_input_event == NULL ||
        SudekiMpControlSeparationGameplayInputFrozen() ||
        !SudekiMpInputBridgePoll(&input)) {
        return TRUE;
    }
    raw_x = (float)input.right_x / 32768.0f;
    raw_y = -(float)input.right_y / 32768.0f;
    magnitude = sqrtf(raw_x * raw_x + raw_y * raw_y);
    if (!isfinite(magnitude) ||
        magnitude <= second_player_controller_camera_deadzone) {
        return TRUE;
    }
    if (magnitude > 1.0f) {
        magnitude = 1.0f;
    }
    scaled_magnitude = (magnitude - second_player_controller_camera_deadzone) /
        (1.0f - second_player_controller_camera_deadzone);
    axis_x = raw_x / sqrtf(raw_x * raw_x + raw_y * raw_y) *
        scaled_magnitude;
    axis_y = raw_y / sqrtf(raw_x * raw_x + raw_y * raw_y) *
        scaled_magnitude;
    if (!isfinite(axis_x) || !isfinite(axis_y)) {
        return FALSE;
    }
    ZeroMemory(&event, sizeof(event));
    event.action = 0x69u; /* CameraL/CameraR normalized by native input. */
    event.magnitude = axis_x;
    /* owner=0 is the native wildcard accepted by CCamera::InputEvent. */
    original_camera_input_event(player_two_camera, &event);
    event.action = 0x6au; /* Native vertical look. */
    event.magnitude = axis_y;
    original_camera_input_event(player_two_camera, &event);
    if (!player_two_native_camera_input_logged) {
        player_two_native_camera_input_logged = TRUE;
        SudekiMpLogFormat(
            "split_screen_render event=player_two_native_camera_input "
            "phase=active camera=0x%08lx actions=0x69,0x6a "
            "policy=right_stick_to_named_camera_native_collision_remains_authoritative\\r\\n",
            (unsigned long)(uintptr_t)player_two_camera);
    }
    return TRUE;
}

static void *current_player_two_party_pointer_slot(void) {
    uint8_t *group;
    uint8_t *slot;

    if (game_base == NULL || player_two_character == NULL ||
        player_two_party_slot >= PARTY_SLOT_COUNT || !readable_memory(
            game_base + RVA_ACTIVE_GROUP_GLOBAL, sizeof(group))) {
        return NULL;
    }
    group = *(uint8_t **)(game_base + RVA_ACTIVE_GROUP_GLOBAL);
    slot = group == NULL ? NULL : group + PARTY_SLOT_ZERO_OFFSET +
        player_two_party_slot * PARTY_SLOT_STRIDE;
    return readable_memory(slot, PARTY_SLOT_STRIDE) &&
        *(void **)slot == player_two_character ? slot : NULL;
}

void *SudekiMpSplitScreenNativeCameraActorFromPartySlot(
    const void *party_slot
) {
    void *character = NULL;

    if (party_slot != NULL) {
        memcpy(&character, party_slot, sizeof(character));
    }
    return character;
}

BOOL SudekiMpSplitScreenNativeCameraWrapperPolicy(
    const void *wrapper,
    const void *party_slot,
    const void *raw_actor,
    const void *embedded_actor,
    const void *resolved_actor,
    const void *leased_actor,
    unsigned int current_actor_generation,
    unsigned int expected_actor_generation,
    BOOL exact_wrapper_vtable
) {
    return wrapper != NULL && party_slot != NULL && raw_actor != NULL &&
        wrapper != party_slot && wrapper != raw_actor &&
        exact_wrapper_vtable && embedded_actor == raw_actor &&
        resolved_actor == raw_actor && leased_actor == raw_actor &&
        current_actor_generation != 0u &&
        current_actor_generation == expected_actor_generation;
}

BOOL SudekiMpSplitScreenNativeCameraWrapperOneShotPolicy(
    BOOL getter_already_attempted,
    BOOL live_group_slot_actor_valid,
    BOOL camera_ownership_valid
) {
    return !getter_already_attempted && live_group_slot_actor_valid &&
        camera_ownership_valid;
}

unsigned int SudekiMpSplitScreenNativeCameraReadinessStagePolicy(
    unsigned int stage,
    BOOL bound
) {
    if (stage == SUDEKIMP_NATIVE_CAMERA_STAGE_IDLE) {
        return bound ? SUDEKIMP_NATIVE_CAMERA_READINESS_REVOKE :
            SUDEKIMP_NATIVE_CAMERA_READINESS_INACTIVE;
    }
    if (stage == SUDEKIMP_NATIVE_CAMERA_STAGE_TARGET_VERIFIED) {
        return bound ? SUDEKIMP_NATIVE_CAMERA_READINESS_REVOKE :
            SUDEKIMP_NATIVE_CAMERA_READINESS_PENDING_STATE;
    }
    if (stage == SUDEKIMP_NATIVE_CAMERA_STAGE_STATE_VERIFIED) {
        return bound ? SUDEKIMP_NATIVE_CAMERA_READINESS_CHECK_STATE :
            SUDEKIMP_NATIVE_CAMERA_READINESS_REVOKE;
    }
    return SUDEKIMP_NATIVE_CAMERA_READINESS_REVOKE;
}

BOOL SudekiMpSplitScreenNativeCameraOwnershipPolicy(
    BOOL current_manager_matches,
    BOOL named_camera_matches,
    BOOL global_camera_matches,
    BOOL player_one_render_state_matches,
    BOOL scene_render_state_matches,
    BOOL render_swap_inactive
) {
    return current_manager_matches && named_camera_matches &&
        global_camera_matches && player_one_render_state_matches &&
        scene_render_state_matches && render_swap_inactive;
}

BOOL SudekiMpSplitScreenNativeCameraIdentityPolicy(
    const void *current_slot,
    const void *bound_slot,
    const void *current_actor,
    const void *bound_actor,
    const void *leased_actor,
    unsigned int current_actor_generation,
    unsigned int bound_actor_generation
) {
    return current_slot != NULL && bound_slot != NULL &&
        current_slot == bound_slot && current_actor != NULL &&
        current_actor == bound_actor && current_actor == leased_actor &&
        current_actor_generation != 0u &&
        current_actor_generation == bound_actor_generation;
}

BOOL SudekiMpSplitScreenNativeCameraMatrixPolicy(const float matrix[16]) {
    float right_length_squared;
    float up_length_squared;
    float forward_length_squared;
    float determinant;
    unsigned int index;

    if (matrix == NULL) {
        return FALSE;
    }
    for (index = 0u; index < 16u; ++index) {
        if (!isfinite(matrix[index])) {
            return FALSE;
        }
    }
    right_length_squared = matrix[0] * matrix[0] +
        matrix[1] * matrix[1] + matrix[2] * matrix[2];
    up_length_squared = matrix[4] * matrix[4] +
        matrix[5] * matrix[5] + matrix[6] * matrix[6];
    forward_length_squared = matrix[8] * matrix[8] +
        matrix[9] * matrix[9] + matrix[10] * matrix[10];
    determinant =
        matrix[0] * (matrix[5] * matrix[10] - matrix[6] * matrix[9]) -
        matrix[1] * (matrix[4] * matrix[10] - matrix[6] * matrix[8]) +
        matrix[2] * (matrix[4] * matrix[9] - matrix[5] * matrix[8]);
    return isfinite(right_length_squared) &&
        isfinite(up_length_squared) &&
        isfinite(forward_length_squared) &&
        isfinite(determinant) &&
        fabsf(matrix[15] - 1.0f) <= 0.001f &&
        right_length_squared > 0.000001f &&
        up_length_squared > 0.000001f &&
        forward_length_squared > 0.000001f &&
        fabsf(determinant) > 0.000001f;
}

unsigned int SudekiMpSplitScreenNativeCameraReleasePolicy(
    BOOL manager_matches,
    BOOL named_identity_known,
    const void *named_camera,
    const void *saved_player_two_camera,
    BOOL global_identity_known,
    const void *global_camera,
    const void *global_render_state,
    const void *saved_player_one_camera,
    const void *saved_player_one_render_state,
    BOOL scene_identity_known,
    const void *scene_render_state,
    const void *saved_player_two_render_state,
    BOOL render_swap_inactive
) {
    if (saved_player_two_camera == NULL ||
        saved_player_two_render_state == NULL ||
        !global_identity_known || !scene_identity_known ||
        !render_swap_inactive) {
        return SUDEKIMP_NATIVE_CAMERA_RELEASE_WAIT;
    }
    if (global_camera == saved_player_two_camera ||
        global_render_state == saved_player_two_render_state ||
        scene_render_state == saved_player_two_render_state) {
        return SUDEKIMP_NATIVE_CAMERA_RELEASE_WAIT;
    }
    if (!manager_matches) {
        return SUDEKIMP_NATIVE_CAMERA_RELEASE_ABANDON;
    }
    if (!named_identity_known) {
        return SUDEKIMP_NATIVE_CAMERA_RELEASE_WAIT;
    }
    if (named_camera != saved_player_two_camera) {
        return SUDEKIMP_NATIVE_CAMERA_RELEASE_ABANDON;
    }
    if (saved_player_one_camera == NULL ||
        saved_player_one_render_state == NULL ||
        global_camera != saved_player_one_camera ||
        global_render_state != saved_player_one_render_state ||
        scene_render_state != saved_player_one_render_state) {
        return SUDEKIMP_NATIVE_CAMERA_RELEASE_WAIT;
    }
    return SUDEKIMP_NATIVE_CAMERA_RELEASE_REMOVE;
}

unsigned int SudekiMpSplitScreenNativeCameraBootstrapPolicy(
    BOOL session_enabled,
    unsigned int stage,
    BOOL phase_eligible,
    BOOL ownership_valid,
    BOOL identity_valid,
    BOOL target_pair_stable,
    BOOL later_frame,
    BOOL active_state_valid,
    BOOL player_one_exploration,
    BOOL player_two_exploration,
    BOOL native_generation_advanced,
    BOOL matrix_valid,
    BOOL readiness_deadline_expired
) {
    if (!session_enabled) {
        return SUDEKIMP_NATIVE_CAMERA_DECISION_FALLBACK;
    }
    if (stage > SUDEKIMP_NATIVE_CAMERA_STAGE_STATE_VERIFIED) {
        return SUDEKIMP_NATIVE_CAMERA_DECISION_REVOKE;
    }
    if (stage == SUDEKIMP_NATIVE_CAMERA_STAGE_IDLE) {
        return phase_eligible && ownership_valid && identity_valid &&
                player_one_exploration ?
            SUDEKIMP_NATIVE_CAMERA_DECISION_SET_TARGET :
            SUDEKIMP_NATIVE_CAMERA_DECISION_FALLBACK;
    }
    if (!ownership_valid || !identity_valid || !target_pair_stable) {
        return SUDEKIMP_NATIVE_CAMERA_DECISION_REVOKE;
    }
    if (stage == SUDEKIMP_NATIVE_CAMERA_STAGE_TARGET_VERIFIED) {
        if (!phase_eligible || !player_one_exploration) {
            return SUDEKIMP_NATIVE_CAMERA_DECISION_WAIT;
        }
        return later_frame ? SUDEKIMP_NATIVE_CAMERA_DECISION_SET_STATE :
            SUDEKIMP_NATIVE_CAMERA_DECISION_WAIT;
    }
    if (!active_state_valid || readiness_deadline_expired) {
        return SUDEKIMP_NATIVE_CAMERA_DECISION_REVOKE;
    }
    if (!phase_eligible || !player_one_exploration) {
        return SUDEKIMP_NATIVE_CAMERA_DECISION_WAIT;
    }
    if (player_two_exploration && native_generation_advanced &&
        !matrix_valid) {
        return SUDEKIMP_NATIVE_CAMERA_DECISION_REVOKE;
    }
    return player_two_exploration && native_generation_advanced &&
            matrix_valid ?
        SUDEKIMP_NATIVE_CAMERA_DECISION_READY :
        SUDEKIMP_NATIVE_CAMERA_DECISION_WAIT;
}

static BOOL render_state_camera_matrix_valid(const void *render_state) {
    const float *matrix;

    if (!readable_memory(render_state, 0xdcu)) {
        return FALSE;
    }
    matrix = (const float *)((const uint8_t *)render_state + 0x90u);
    return SudekiMpSplitScreenNativeCameraMatrixPolicy(matrix);
}

static BOOL player_two_native_matrix_valid(void) {
    return render_state_camera_matrix_valid(player_two_render_state);
}

static void arm_native_camera_recovery_window(void) {
    player_two_native_camera_recovery_armed = TRUE;
    player_two_native_camera_recovery_eligible_frames = 0u;
    player_two_native_camera_recovery_last_frame = split_render_frame_sequence;
}

static void disarm_native_camera_recovery_window(void) {
    player_two_native_camera_recovery_armed = FALSE;
    player_two_native_camera_recovery_eligible_frames = 0u;
    player_two_native_camera_recovery_last_frame = split_render_frame_sequence;
}

static BOOL native_camera_recovery_deadline_expired(BOOL eligible) {
    if (!player_two_native_camera_recovery_armed || !eligible) {
        return FALSE;
    }
    if (player_two_native_camera_recovery_last_frame !=
            split_render_frame_sequence) {
        player_two_native_camera_recovery_last_frame =
            split_render_frame_sequence;
        if (player_two_native_camera_recovery_eligible_frames < UINT_MAX) {
            ++player_two_native_camera_recovery_eligible_frames;
        }
    }
    return player_two_native_camera_recovery_eligible_frames >=
        NATIVE_CAMERA_READINESS_FRAME_LIMIT;
}

BOOL SudekiMpSplitScreenObserveNativeCameraGeneration(
    unsigned short *baseline,
    unsigned short generation,
    BOOL manual_fallback_write
) {
    if (baseline == NULL) {
        return FALSE;
    }
    if (manual_fallback_write) {
        *baseline = generation;
        return FALSE;
    }
    return generation != *baseline;
}

static BOOL native_camera_phase_eligible(void) {
    void *group;

    if (game_base == NULL || group_players_in_combat == NULL ||
        !readable_memory(
            game_base + RVA_ACTIVE_GROUP_GLOBAL,
            sizeof(group))) {
        if (player_two_native_camera_stage !=
                SUDEKIMP_NATIVE_CAMERA_STAGE_IDLE) {
            request_native_camera_manual_recreation(
                "native_phase_active_group_global_unreadable");
        }
        return FALSE;
    }
    group = *(void **)(game_base + RVA_ACTIVE_GROUP_GLOBAL);
    if (!readable_memory(group, 0xd5u)) {
        if (player_two_native_camera_stage !=
                SUDEKIMP_NATIVE_CAMERA_STAGE_IDLE) {
            request_native_camera_manual_recreation(
                "native_phase_active_group_unreadable");
        }
        return FALSE;
    }
    return group_players_in_combat(group) == 0u &&
        player_skill_render_states[0] == NULL &&
        player_skill_render_states[1] == NULL &&
        current_spirit_presentation_state() == 0;
}

static BOOL native_camera_current_actor_identity(
    void **slot_result,
    void **actor_result,
    uint32_t *generation_result
) {
    SudekiMpPlayerStatehood *statehood = SudekiMpPlayerStatehoodRuntime();
    const SudekiMpPlayerLease *lease =
        statehood == NULL ? NULL : &statehood->players[1];
    void *slot = current_player_two_party_pointer_slot();
    void *actor = SudekiMpSplitScreenNativeCameraActorFromPartySlot(slot);

    if (slot_result != NULL) {
        *slot_result = slot;
    }
    if (actor_result != NULL) {
        *actor_result = actor;
    }
    if (generation_result != NULL) {
        *generation_result = lease == NULL ? 0u : lease->actor_generation;
    }
    return lease != NULL && lease->human_present &&
        actor == player_two_character &&
        lease->actor == (uintptr_t)actor &&
        lease->actor_generation != 0u;
}

static BOOL native_camera_bound_identity_valid(void) {
    void *slot = NULL;
    void *actor = NULL;
    uint32_t generation = 0u;

    if (!native_camera_current_actor_identity(
            &slot, &actor, &generation)) {
        return FALSE;
    }
    return SudekiMpSplitScreenNativeCameraIdentityPolicy(
        slot,
        player_two_native_camera_target_slot,
        actor,
        player_two_native_camera_target_actor,
        actor,
        generation,
        player_two_native_camera_actor_generation);
}

static BOOL native_camera_target_pair_stable(void) {
    void *position_target;
    void *look_at_target;

    if (!readable_memory(
            player_two_camera,
            CAMERA_LOOK_AT_TARGET_OFFSET + sizeof(void *))) {
        return FALSE;
    }
    position_target = *(void **)((uint8_t *)player_two_camera +
        CAMERA_POSITION_TARGET_OFFSET);
    look_at_target = *(void **)((uint8_t *)player_two_camera +
        CAMERA_LOOK_AT_TARGET_OFFSET);
    return position_target != NULL && look_at_target != NULL &&
        position_target == player_two_native_camera_position_target &&
        look_at_target == player_two_native_camera_look_at_target;
}

static BOOL native_camera_ownership_valid(void) {
    void *manager = current_camera_manager();
    void *named_camera = NULL;
    void *global_camera = NULL;
    void *global_render_state = NULL;
    void **scene_slot = NULL;
    BOOL manager_matches = manager != NULL &&
        manager == second_player_camera_manager;

    if (manager_matches && camera_manager_get_camera != NULL) {
        named_camera = camera_manager_get_camera(
            manager, second_player_camera_name);
        global_camera = current_render_camera(manager);
        if (readable_memory(global_camera, 0x38u)) {
            global_render_state = *(void **)((uint8_t *)global_camera +
                CAMERA_RENDER_STATE_OFFSET);
        }
        scene_slot = current_scene_render_camera_slot();
    }
    return SudekiMpSplitScreenNativeCameraOwnershipPolicy(
        manager_matches,
        named_camera == player_two_camera,
        global_camera == player_one_camera,
        global_render_state == player_one_render_state,
        scene_slot != NULL && *scene_slot == player_one_render_state,
        !render_only_swap_active);
}

static void request_native_camera_manual_recreation(const char *reason) {
    if (!native_second_player_camera_collision_session_enabled &&
        player_two_native_camera_recreate_requested) {
        return;
    }
    native_second_player_camera_collision_session_enabled = FALSE;
    player_two_native_camera_recreate_requested = TRUE;
    player_two_native_camera_ready = FALSE;
    player_two_camera_transform_initialized = FALSE;
    disarm_native_camera_recovery_window();
    player_two_view_requested = FALSE;
    invalidate_dual_frame_cache();
    SudekiMpLogFormat(
        "split_screen_render event=player_two_native_camera "
        "phase=session_disabled reason=%s "
        "action=request_poll_boundary_camera_recreation "
        "fallback=manual_translated_orbit_no_native_collision\r\n",
        reason == NULL ? "unspecified" : reason);
}

static BOOL native_camera_resident_health_valid(void) {
    void *active_state;
    void *active_state_data;

    if (player_two_native_camera_stage ==
            SUDEKIMP_NATIVE_CAMERA_STAGE_IDLE) {
        return TRUE;
    }
    if (!native_second_player_camera_collision_session_enabled ||
        camera_manager_get_camera == NULL ||
        camera_manager_get_camera_mode == NULL ||
        camera_manager_set_camera_target == NULL ||
        camera_manager_set_camera_state == NULL ||
        group_players_in_combat == NULL ||
        second_player_camera_manager == NULL ||
        player_one_camera == NULL || player_two_camera == NULL ||
        !readable_memory(player_one_camera, 0x38u) ||
        !readable_memory(
            player_two_camera,
            CAMERA_LOOK_AT_TARGET_OFFSET + sizeof(void *)) ||
        !readable_memory(player_one_render_state, 0xdcu) ||
        !readable_memory(player_two_render_state, 0xdcu)) {
        request_native_camera_manual_recreation(
            "resident_required_api_memory_or_owner_lost");
        return FALSE;
    }
    if (!native_camera_bound_identity_valid() ||
        !native_camera_ownership_valid() ||
        !native_camera_target_pair_stable()) {
        request_native_camera_manual_recreation(
            "resident_ownership_actor_generation_or_target_lost");
        return FALSE;
    }
    if (player_two_native_camera_stage ==
            SUDEKIMP_NATIVE_CAMERA_STAGE_STATE_VERIFIED) {
        active_state = *(void **)((uint8_t *)player_two_camera +
            CAMERA_ACTIVE_STATE_OFFSET);
        active_state_data = *(void **)((uint8_t *)player_two_camera +
            CAMERA_ACTIVE_STATE_DATA_OFFSET);
        if (!player_two_native_camera_bound ||
            active_state == NULL || active_state_data == NULL) {
            request_native_camera_manual_recreation(
                "resident_active_state_or_bound_marker_lost");
            return FALSE;
        }
    } else if (player_two_native_camera_stage !=
            SUDEKIMP_NATIVE_CAMERA_STAGE_TARGET_VERIFIED) {
        request_native_camera_manual_recreation(
            "resident_stage_invalid");
        return FALSE;
    }
    /* Besides combat state, this verifies that the active-group object is
     * structurally readable. Ineligible combat/skill/spirit phases remain a
     * healthy paused session rather than a failure. */
    (void)native_camera_phase_eligible();
    return native_second_player_camera_collision_session_enabled;
}

static int native_camera_get_mode_logged(
    const char *operation,
    const char *camera_name
) {
    int mode;

    SudekiMpLogFormat(
        "split_screen_render event=player_two_native_camera_call "
        "function=GetCameraMode phase=before operation=%s camera=%s\r\n",
        operation,
        camera_name == NULL ? "<global>" : camera_name);
    mode = camera_manager_get_camera_mode(
        second_player_camera_manager, camera_name);
    SudekiMpLogFormat(
        "split_screen_render event=player_two_native_camera_call "
        "function=GetCameraMode phase=after operation=%s camera=%s "
        "result=%d\r\n",
        operation,
        camera_name == NULL ? "<global>" : camera_name,
        mode);
    return mode;
}

static void *native_camera_target_group_for_slot(const void *party_slot) {
    void *group;
    const uint8_t *expected_slot;

    if (game_base == NULL || party_slot == NULL ||
        player_two_party_slot >= PARTY_SLOT_COUNT || !readable_memory(
            game_base + RVA_ACTIVE_GROUP_GLOBAL, sizeof(group))) {
        return NULL;
    }
    group = *(void **)(game_base + RVA_ACTIVE_GROUP_GLOBAL);
    if (!readable_memory(group, 0xd5u)) {
        return NULL;
    }
    expected_slot = (const uint8_t *)group + PARTY_SLOT_ZERO_OFFSET +
        player_two_party_slot * PARTY_SLOT_STRIDE;
    return expected_slot == party_slot ? group : NULL;
}

static void *native_camera_get_target_wrapper_logged(
    void *group,
    const void *party_slot,
    const void *raw_actor
) {
    void *wrapper;

    if (!SudekiMpSplitScreenNativeCameraWrapperOneShotPolicy(
            player_two_native_camera_wrapper_getter_attempted,
            group != NULL && party_slot != NULL && raw_actor != NULL,
            native_camera_ownership_valid())) {
        SudekiMpLogWrite(
            "split_screen_render event=player_two_native_camera_wrapper "
            "phase=get_rejected reason=one_shot_or_live_precondition_failed "
            "action=manual_fallback_no_second_allocation\r\n");
        request_native_camera_manual_recreation(
            player_two_native_camera_wrapper_getter_attempted ?
                "target_wrapper_one_shot_already_consumed" :
                "target_wrapper_live_precondition_failed");
        return NULL;
    }
    player_two_native_camera_wrapper_getter_attempted = TRUE;

    SudekiMpLogFormat(
        "split_screen_render event=player_two_native_camera_wrapper "
        "phase=get_before group=0x%08lx party_slot=%u "
        "slot_address=0x%08lx raw_actor=0x%08lx\r\n",
        (unsigned long)(uintptr_t)group,
        player_two_party_slot,
        (unsigned long)(uintptr_t)party_slot,
        (unsigned long)(uintptr_t)raw_actor);
    wrapper = group_players_get_player_group(
        group, (int)player_two_party_slot);
    SudekiMpLogFormat(
        "split_screen_render event=player_two_native_camera_wrapper "
        "phase=get_after group=0x%08lx party_slot=%u "
        "wrapper=0x%08lx raw_actor=0x%08lx\r\n",
        (unsigned long)(uintptr_t)group,
        player_two_party_slot,
        (unsigned long)(uintptr_t)wrapper,
        (unsigned long)(uintptr_t)raw_actor);
    SudekiMpLogWrite(
        "split_screen_render event=player_two_native_camera_wrapper "
        "phase=one_shot_consumed "
        "policy=engine_ptrobj_registry_delete_all_owns_wrapper_"
        "mod_never_caches_reuses_or_calls_native_destructor\r\n");
    return wrapper;
}

static BOOL native_camera_resolve_target_wrapper(
    void *wrapper,
    void **embedded_actor_result,
    void **resolved_actor_result,
    BOOL *exact_vtable_result
) {
    void *tracked_entity[3] = {NULL, NULL, NULL};
    void **vtable;
    void *embedded_actor = NULL;
    void *resolved_actor = NULL;
    BOOL exact_vtable = FALSE;
    BOOL resolved = FALSE;
    uint8_t resolver_result = 0u;

    if (readable_memory(wrapper, 0x18u)) {
        vtable = *(void ***)wrapper;
        exact_vtable = vtable == (void **)(
            game_base + RVA_GEL_GROUP_PTR_VTABLE) &&
            readable_memory(vtable, 0x30u) &&
            vtable[0] == game_base +
                RVA_GEL_GROUP_PTR_DELETING_DESTRUCTOR &&
            vtable[4] == game_base + RVA_GEL_GROUP_PTR_GET_RAW_ENTITY &&
            vtable[11] == game_base + RVA_GEL_GROUP_PTR_TYPE_NAME;
        embedded_actor = *(void **)((uint8_t *)wrapper + 0x0cu);
        if (exact_vtable) {
            resolver_result = gel_pointer_resolve_entity(
                wrapper, tracked_entity);
            resolved = resolver_result != 0u;
            resolved_actor = tracked_entity[0];
            tracked_entity_cleanup(tracked_entity);
        }
    }
    if (embedded_actor_result != NULL) {
        *embedded_actor_result = embedded_actor;
    }
    if (resolved_actor_result != NULL) {
        *resolved_actor_result = resolved_actor;
    }
    if (exact_vtable_result != NULL) {
        *exact_vtable_result = exact_vtable;
    }
    SudekiMpLogFormat(
        "split_screen_render event=player_two_native_camera_wrapper "
        "phase=provenance wrapper=0x%08lx embedded_actor=0x%08lx "
        "resolved_actor=0x%08lx exact_vtable=%u resolver_al=%u "
        "resolved_boolean=%u\r\n",
        (unsigned long)(uintptr_t)wrapper,
        (unsigned long)(uintptr_t)embedded_actor,
        (unsigned long)(uintptr_t)resolved_actor,
        exact_vtable,
        (unsigned int)resolver_result,
        resolved);
    return resolved;
}

static BOOL native_camera_set_target_logged(
    void *target_wrapper,
    void *target_character
) {
    BOOL ownership_valid;

    if (target_wrapper == NULL) {
        request_native_camera_manual_recreation(
            "set_target_missing_owned_gel_group_ptr_wrapper");
        return FALSE;
    }
    SudekiMpLogFormat(
        "split_screen_render event=player_two_native_camera_call "
        "function=SetCameraTarget phase=before camera=%s "
        "wrapper=0x%08lx resolved_actor=0x%08lx\r\n",
        second_player_camera_name,
        (unsigned long)(uintptr_t)target_wrapper,
        (unsigned long)(uintptr_t)target_character);
    camera_manager_set_camera_target(
        second_player_camera_manager,
        second_player_camera_name,
        target_wrapper);
    ownership_valid = native_camera_ownership_valid();
    SudekiMpLogFormat(
        "split_screen_render event=player_two_native_camera_call "
        "function=SetCameraTarget phase=after camera=%s "
        "wrapper=0x%08lx resolved_actor=0x%08lx\r\n",
        second_player_camera_name,
        (unsigned long)(uintptr_t)target_wrapper,
        (unsigned long)(uintptr_t)target_character);
    return ownership_valid;
}

static void native_camera_set_state_logged(float transition_b) {
    SudekiMpLogFormat(
        "split_screen_render event=player_two_native_camera_call "
        "function=SetCameraState phase=before camera=%s state=%s "
        "transition_a_bits=0x%08lx transition_flag=0 "
        "transition_b_bits=0x%08lx\r\n",
        second_player_camera_name,
        exploration_camera_state_name,
        (unsigned long)float_bits(0.0f),
        (unsigned long)float_bits(transition_b));
    camera_manager_set_camera_state(
        second_player_camera_manager,
        second_player_camera_name,
        exploration_camera_state_name,
        0.0f,
        FALSE,
        transition_b);
    SudekiMpLogFormat(
        "split_screen_render event=player_two_native_camera_call "
        "function=SetCameraState phase=after camera=%s state=%s\r\n",
        second_player_camera_name,
        exploration_camera_state_name);
}

unsigned int SudekiMpSplitScreenTemporaryCameraPolicy(
    BOOL settled_temporary_zone,
    int player_one_camera_mode,
    BOOL native_player_two_ready
) {
    if (!settled_temporary_zone) {
        return SUDEKIMP_TEMP_CAMERA_OUTSIDE;
    }
    if (player_one_camera_mode == CAMERA_MODE_EXPLORATION &&
        native_player_two_ready) {
        return SUDEKIMP_TEMP_CAMERA_NATIVE_PLAYER_TWO;
    }
    return SUDEKIMP_TEMP_CAMERA_SHARED_FULL_WIDTH;
}

static int current_player_one_camera_mode(void) {
    int mode;

    if (!native_second_player_camera_collision_session_enabled ||
        camera_manager_get_camera_mode == NULL ||
        group_players_in_combat == NULL ||
        second_player_camera_manager == NULL) {
        if (player_two_native_camera_stage !=
                SUDEKIMP_NATIVE_CAMERA_STAGE_IDLE) {
            request_native_camera_manual_recreation(
                "temporary_mode_required_api_or_manager_lost");
        }
        return -1;
    }
    if (!native_camera_ownership_valid()) {
        if (player_two_native_camera_stage !=
                SUDEKIMP_NATIVE_CAMERA_STAGE_IDLE) {
            request_native_camera_manual_recreation(
                "temporary_mode_camera_ownership_lost");
        }
        return -1;
    }
    mode = native_camera_get_mode_logged("temporary_policy", NULL);
    if (!native_camera_ownership_valid()) {
        if (player_two_native_camera_stage !=
                SUDEKIMP_NATIVE_CAMERA_STAGE_IDLE) {
            request_native_camera_manual_recreation(
                "temporary_get_mode_changed_camera_ownership");
        }
        return -1;
    }
    return mode;
}

static void set_player_two_temporary_camera_policy(
    unsigned int policy,
    int player_one_camera_mode,
    const char *reason
) {
    unsigned int previous = player_two_temporary_camera_policy;
    const char *presentation;

    if (policy > SUDEKIMP_TEMP_CAMERA_NATIVE_PLAYER_TWO ||
        policy == previous) {
        return;
    }
    player_two_temporary_camera_policy = policy;
    invalidate_dual_frame_cache();
    if (policy == SUDEKIMP_TEMP_CAMERA_SHARED_FULL_WIDTH) {
        player_two_view_requested = FALSE;
        presentation = "shared_native_full_width";
    } else if (policy == SUDEKIMP_TEMP_CAMERA_NATIVE_PLAYER_TWO) {
        presentation = "split_native_exploration";
    } else {
        presentation = "ordinary_outdoor_policy";
    }
    SudekiMpLogFormat(
        "split_screen_render event=temporary_camera_policy "
        "previous=%u current=%u player_one_mode=%d presentation=%s "
        "reason=%s "
        "policy=native_p2_only_after_exact_exploration_readiness_otherwise_shared_full_width\r\n",
        previous,
        policy,
        player_one_camera_mode,
        presentation,
        reason == NULL ? "unspecified" : reason);
}

static BOOL initialize_player_two_native_camera(void) {
    void *target_slot = NULL;
    void *target_character = NULL;
    void *active_state;
    void *active_state_data;
    void *position_target;
    void *look_at_target;
    uint32_t actor_generation = 0u;
    unsigned int decision;
    BOOL phase_eligible;
    BOOL ownership_valid;
    BOOL identity_valid;
    BOOL target_pair_stable;
    BOOL later_frame;
    float transition_b;
    int player_one_mode;
    int player_two_mode;

    if (!native_second_player_camera_collision_session_enabled ||
        player_two_native_camera_bound) {
        return FALSE;
    }
    if (camera_manager_set_camera_target == NULL ||
        group_players_get_player_group == NULL ||
        gel_pointer_resolve_entity == NULL ||
        tracked_entity_cleanup == NULL ||
        camera_manager_get_camera_mode == NULL ||
        camera_manager_set_camera_state == NULL ||
        camera_manager_get_camera == NULL ||
        group_players_in_combat == NULL ||
        second_player_camera_manager == NULL ||
        player_one_camera == NULL || player_two_camera == NULL ||
        !readable_memory(player_two_camera,
            CAMERA_LOOK_AT_TARGET_OFFSET + sizeof(void *)) ||
        !readable_memory(player_two_render_state, 0xdcu)) {
        if (player_two_native_camera_stage !=
                SUDEKIMP_NATIVE_CAMERA_STAGE_IDLE) {
            request_native_camera_manual_recreation(
                "bootstrap_required_api_memory_or_owner_lost");
        }
        return FALSE;
    }
    phase_eligible = native_camera_phase_eligible();
    ownership_valid = native_camera_ownership_valid();
    if (player_two_native_camera_stage ==
            SUDEKIMP_NATIVE_CAMERA_STAGE_IDLE) {
        identity_valid = native_camera_current_actor_identity(
            &target_slot, &target_character, &actor_generation) &&
            SudekiMpSplitScreenNativeCameraIdentityPolicy(
                target_slot, target_slot,
                target_character, target_character, target_character,
                actor_generation, actor_generation);
    } else {
        identity_valid = native_camera_bound_identity_valid();
        target_slot = current_player_two_party_pointer_slot();
        target_character = SudekiMpSplitScreenNativeCameraActorFromPartySlot(
            target_slot);
        actor_generation = player_two_native_camera_actor_generation;
    }
    target_pair_stable = native_camera_target_pair_stable();
    later_frame = split_render_frame_sequence !=
        player_two_native_camera_target_frame;
    player_one_mode = -1;
    player_two_mode = -1;
    if (player_two_native_camera_stage !=
            SUDEKIMP_NATIVE_CAMERA_STAGE_IDLE &&
        (!identity_valid || !target_pair_stable)) {
        request_native_camera_manual_recreation(
            identity_valid ? "bootstrap_target_pair_revoked" :
                "bootstrap_actor_identity_revoked");
        return FALSE;
    }
    if (player_two_native_camera_stage !=
            SUDEKIMP_NATIVE_CAMERA_STAGE_IDLE &&
        !ownership_valid) {
        request_native_camera_manual_recreation(
            "bootstrap_camera_ownership_revoked");
        return FALSE;
    }
    if (!phase_eligible || !ownership_valid || !identity_valid) {
        return FALSE;
    }
    if (!native_camera_ownership_valid()) {
        return FALSE;
    }
    player_one_mode = native_camera_get_mode_logged(
        player_two_native_camera_stage ==
                SUDEKIMP_NATIVE_CAMERA_STAGE_IDLE ?
            "before_set_target" : "before_set_state",
        NULL);
    if (!native_camera_ownership_valid()) {
        request_native_camera_manual_recreation(
            "get_mode_changed_camera_ownership");
        return FALSE;
    }
    if (player_one_mode != CAMERA_MODE_EXPLORATION) {
        return FALSE;
    }
    decision = SudekiMpSplitScreenNativeCameraBootstrapPolicy(
        TRUE,
        player_two_native_camera_stage,
        TRUE,
        TRUE,
        identity_valid,
        target_pair_stable,
        later_frame,
        FALSE,
        TRUE,
        FALSE,
        FALSE,
        FALSE,
        FALSE);
    if (decision == SUDEKIMP_NATIVE_CAMERA_DECISION_REVOKE) {
        request_native_camera_manual_recreation(
            "bootstrap_policy_revoked");
        return FALSE;
    }
    if (decision != SUDEKIMP_NATIVE_CAMERA_DECISION_SET_TARGET &&
        decision != SUDEKIMP_NATIVE_CAMERA_DECISION_SET_STATE) {
        return FALSE;
    }

    if (decision == SUDEKIMP_NATIVE_CAMERA_DECISION_SET_TARGET) {
        void *pre_slot = NULL;
        void *pre_actor = NULL;
        void *target_group = NULL;
        void *target_wrapper = NULL;
        void *embedded_actor = NULL;
        void *resolved_actor = NULL;
        void *post_slot = NULL;
        void *post_actor = NULL;
        uint32_t pre_generation = 0u;
        uint32_t post_generation = 0u;
        BOOL exact_wrapper_vtable = FALSE;
        BOOL setter_ownership_valid;
        BOOL wrapper_resolved;

        if (!native_camera_current_actor_identity(
                &pre_slot, &pre_actor, &pre_generation) ||
            !SudekiMpSplitScreenNativeCameraIdentityPolicy(
                pre_slot, target_slot,
                pre_actor, target_character, pre_actor,
                pre_generation, actor_generation) ||
            !native_camera_ownership_valid()) {
            return FALSE;
        }
        target_group = native_camera_target_group_for_slot(pre_slot);
        if (target_group == NULL) {
            request_native_camera_manual_recreation(
                "target_wrapper_active_group_or_slot_lost");
            return FALSE;
        }
        player_two_native_camera_attempted = TRUE;
        target_wrapper = native_camera_get_target_wrapper_logged(
            target_group, pre_slot, pre_actor);
        if (target_wrapper == NULL) {
            request_native_camera_manual_recreation(
                "target_wrapper_factory_or_one_shot_failed");
            return FALSE;
        }
        wrapper_resolved = native_camera_resolve_target_wrapper(
            target_wrapper,
            &embedded_actor,
            &resolved_actor,
            &exact_wrapper_vtable);
        /* Re-resolve every live identity immediately before the setter. The
         * slot/raw actor remain provenance only; only this fresh, exact native
         * GELGroupPtr is passed to SetCameraTarget. */
        if (!wrapper_resolved ||
            !native_camera_current_actor_identity(
                &pre_slot, &pre_actor, &pre_generation) ||
            native_camera_target_group_for_slot(pre_slot) != target_group ||
            !SudekiMpSplitScreenNativeCameraIdentityPolicy(
                pre_slot, target_slot,
                pre_actor, target_character, pre_actor,
                pre_generation, actor_generation) ||
            !SudekiMpSplitScreenNativeCameraWrapperPolicy(
                target_wrapper, pre_slot, pre_actor,
                embedded_actor, resolved_actor, pre_actor,
                pre_generation, actor_generation,
                exact_wrapper_vtable) ||
            !native_camera_ownership_valid()) {
            request_native_camera_manual_recreation(
                "target_wrapper_provenance_rejected");
            return FALSE;
        }
        setter_ownership_valid = native_camera_set_target_logged(
            target_wrapper, target_character);
        /* SetCameraTarget synchronously resolves/copies its actor target. Do
         * not cache, reuse, dereference, or explicitly destroy the wrapper;
         * the engine PtrObj registry owns its delete-all lifetime. */
        target_wrapper = NULL;
        if (!setter_ownership_valid || !native_camera_ownership_valid()) {
            request_native_camera_manual_recreation(
                "set_target_changed_camera_ownership");
            return FALSE;
        }
        position_target = *(void **)((uint8_t *)player_two_camera +
            CAMERA_POSITION_TARGET_OFFSET);
        look_at_target = *(void **)((uint8_t *)player_two_camera +
            CAMERA_LOOK_AT_TARGET_OFFSET);
        if (position_target == NULL || look_at_target == NULL ||
            !native_camera_current_actor_identity(
                &post_slot, &post_actor, &post_generation) ||
            !SudekiMpSplitScreenNativeCameraIdentityPolicy(
                post_slot, target_slot,
                post_actor, target_character, post_actor,
                post_generation, actor_generation)) {
            request_native_camera_manual_recreation(
                "set_target_postcondition_failed");
            return FALSE;
        }
        player_two_native_camera_target_slot = target_slot;
        player_two_native_camera_target_actor = target_character;
        player_two_native_camera_actor_generation = actor_generation;
        player_two_native_camera_position_target = position_target;
        player_two_native_camera_look_at_target = look_at_target;
        player_two_native_camera_target_frame = split_render_frame_sequence;
        player_two_native_camera_stage =
            SUDEKIMP_NATIVE_CAMERA_STAGE_TARGET_VERIFIED;
        SudekiMpLogFormat(
            "split_screen_render event=player_two_native_camera "
            "phase=target_verified target_slot=0x%08lx target=0x%08lx "
            "actor_generation=%lu position_target=0x%08lx "
            "look_at_target=0x%08lx render_frame=%u "
            "policy=state_activation_deferred_until_later_stable_frame\r\n",
            (unsigned long)(uintptr_t)target_slot,
            (unsigned long)(uintptr_t)target_character,
            (unsigned long)actor_generation,
            (unsigned long)(uintptr_t)position_target,
            (unsigned long)(uintptr_t)look_at_target,
            split_render_frame_sequence);
        return TRUE;
    }

    player_two_mode = native_camera_get_mode_logged(
        "before_set_state", second_player_camera_name);
    if (!native_camera_ownership_valid() ||
        !native_camera_bound_identity_valid() ||
        !native_camera_target_pair_stable()) {
        request_native_camera_manual_recreation(
            "pre_state_stability_recheck_failed");
        return FALSE;
    }
    memcpy(&transition_b, &native_exploration_transition_b_bits,
        sizeof(transition_b));
    native_camera_set_state_logged(transition_b);
    if (!native_camera_ownership_valid() ||
        !native_camera_bound_identity_valid() ||
        !native_camera_target_pair_stable()) {
        request_native_camera_manual_recreation(
            "set_state_changed_camera_ownership_or_target");
        return FALSE;
    }
    active_state = *(void **)((uint8_t *)player_two_camera +
        CAMERA_ACTIVE_STATE_OFFSET);
    active_state_data = *(void **)((uint8_t *)player_two_camera +
        CAMERA_ACTIVE_STATE_DATA_OFFSET);
    player_one_mode = native_camera_get_mode_logged(
        "after_set_state", NULL);
    if (!native_camera_ownership_valid() ||
        !native_camera_bound_identity_valid() ||
        !native_camera_target_pair_stable()) {
        request_native_camera_manual_recreation(
            "set_state_global_mode_changed_ownership_identity_or_target");
        return FALSE;
    }
    player_two_mode = native_camera_get_mode_logged(
        "after_set_state", second_player_camera_name);
    if (!native_camera_ownership_valid() ||
        player_one_mode != CAMERA_MODE_EXPLORATION ||
        active_state == NULL || active_state_data == NULL ||
        (player_two_mode != CAMERA_MODE_EXPLORATION &&
         player_two_mode != CAMERA_MODE_STATE_TRANSITION)) {
        request_native_camera_manual_recreation(
            "set_state_postcondition_failed");
        return FALSE;
    }
    player_two_native_camera_bound = TRUE;
    player_two_native_camera_stage =
        SUDEKIMP_NATIVE_CAMERA_STAGE_STATE_VERIFIED;
    player_two_native_camera_state_frame = split_render_frame_sequence;
    arm_native_camera_recovery_window();
    player_two_native_camera_start_generation = *(uint16_t *)(
        (uint8_t *)player_two_render_state +
        RENDER_STATE_GENERATION_OFFSET);
    SudekiMpLogFormat(
        "split_screen_render event=player_two_native_camera "
        "phase=state_verified player_one_mode=%d player_two_mode=%d "
        "target_slot=0x%08lx target=0x%08lx actor_generation=%lu "
        "generation=%u policy=wait_for_independent_native_matrix\r\n",
        player_one_mode,
        player_two_mode,
        (unsigned long)(uintptr_t)player_two_native_camera_target_slot,
        (unsigned long)(uintptr_t)player_two_native_camera_target_actor,
        (unsigned long)player_two_native_camera_actor_generation,
        (unsigned int)player_two_native_camera_start_generation);
    return TRUE;
}

static BOOL player_two_native_camera_is_ready(void) {
    void *active_state;
    void *active_state_data;
    uint16_t generation;
    unsigned int decision;
    unsigned int stage_policy;
    BOOL generation_advanced;
    BOOL matrix_valid;
    BOOL readiness_deadline_expired;
    BOOL phase_eligible;
    BOOL eligible_recovery_frame;
    BOOL was_ready;
    int player_one_mode;
    int player_two_mode;

    if (!native_second_player_camera_collision_session_enabled) {
        player_two_native_camera_ready = FALSE;
        return FALSE;
    }
    stage_policy = SudekiMpSplitScreenNativeCameraReadinessStagePolicy(
        player_two_native_camera_stage,
        player_two_native_camera_bound);
    if (stage_policy !=
            SUDEKIMP_NATIVE_CAMERA_READINESS_CHECK_STATE) {
        player_two_native_camera_ready = FALSE;
        if (stage_policy == SUDEKIMP_NATIVE_CAMERA_READINESS_REVOKE) {
            request_native_camera_manual_recreation(
                "ready_unexpected_stage_bound_combination");
        }
        return FALSE;
    }
    if (camera_manager_get_camera == NULL ||
        camera_manager_get_camera_mode == NULL ||
        camera_manager_set_camera_target == NULL ||
        camera_manager_set_camera_state == NULL ||
        group_players_in_combat == NULL ||
        second_player_camera_manager == NULL ||
        player_one_camera == NULL || player_two_camera == NULL ||
        player_one_render_state == NULL || player_two_render_state == NULL ||
        !readable_memory(player_two_camera,
            CAMERA_LOOK_AT_TARGET_OFFSET + sizeof(void *)) ||
        !readable_memory(player_two_render_state, 0xdcu)) {
        request_native_camera_manual_recreation(
            "ready_required_api_memory_or_owner_lost");
        return FALSE;
    }
    was_ready = player_two_native_camera_ready;
    if (!native_camera_bound_identity_valid()) {
        request_native_camera_manual_recreation(
            "ready_actor_slot_or_generation_revoked");
        return FALSE;
    }
    if (!native_camera_ownership_valid() ||
        !native_camera_target_pair_stable()) {
        request_native_camera_manual_recreation(
            "ready_camera_ownership_or_target_revoked");
        return FALSE;
    }
    player_one_mode = native_camera_get_mode_logged(
        "readiness", NULL);
    if (!native_camera_ownership_valid()) {
        request_native_camera_manual_recreation(
            "ready_global_mode_changed_camera_ownership");
        return FALSE;
    }
    player_two_mode = native_camera_get_mode_logged(
        "readiness", second_player_camera_name);
    if (!native_camera_ownership_valid() ||
        !native_camera_bound_identity_valid() ||
        !native_camera_target_pair_stable()) {
        request_native_camera_manual_recreation(
            "ready_named_mode_changed_ownership_identity_or_target");
        return FALSE;
    }
    generation = *(uint16_t *)((uint8_t *)player_two_render_state +
        RENDER_STATE_GENERATION_OFFSET);
    active_state = *(void **)((uint8_t *)player_two_camera +
        CAMERA_ACTIVE_STATE_OFFSET);
    active_state_data = *(void **)((uint8_t *)player_two_camera +
        CAMERA_ACTIVE_STATE_DATA_OFFSET);
    if (active_state == NULL || active_state_data == NULL) {
        request_native_camera_manual_recreation(
            "ready_active_state_lost");
        return FALSE;
    }
    generation_advanced = SudekiMpSplitScreenObserveNativeCameraGeneration(
        &player_two_native_camera_start_generation,
        generation,
        FALSE);
    matrix_valid = player_two_native_matrix_valid();
    if (!matrix_valid) {
        player_two_camera_transform_initialized = FALSE;
    }
    phase_eligible = native_camera_phase_eligible();
    eligible_recovery_frame = phase_eligible &&
        player_one_mode == CAMERA_MODE_EXPLORATION;
    if (was_ready &&
        (!eligible_recovery_frame ||
         player_two_mode != CAMERA_MODE_EXPLORATION ||
         !generation_advanced || !matrix_valid)) {
        arm_native_camera_recovery_window();
    }
    readiness_deadline_expired =
        native_camera_recovery_deadline_expired(
            eligible_recovery_frame);
    decision = SudekiMpSplitScreenNativeCameraBootstrapPolicy(
        TRUE,
        player_two_native_camera_stage,
        phase_eligible,
        TRUE,
        TRUE,
        TRUE,
        TRUE,
        TRUE,
        player_one_mode == CAMERA_MODE_EXPLORATION,
        player_two_mode == CAMERA_MODE_EXPLORATION,
        generation_advanced,
        matrix_valid,
        readiness_deadline_expired);
    if (decision == SUDEKIMP_NATIVE_CAMERA_DECISION_REVOKE) {
        request_native_camera_manual_recreation(
            player_two_mode == CAMERA_MODE_EXPLORATION &&
                    generation_advanced && !matrix_valid ?
                "native_generation_produced_invalid_matrix" :
            readiness_deadline_expired ?
                "native_readiness_deadline_expired" :
                "ready_state_postcondition_revoked");
        return FALSE;
    }
    player_two_native_camera_ready =
        decision == SUDEKIMP_NATIVE_CAMERA_DECISION_READY;
    if (player_two_native_camera_ready) {
        disarm_native_camera_recovery_window();
    }
    if (player_two_native_camera_ready &&
        !player_two_native_camera_ready_logged) {
        player_two_native_camera_ready_logged = TRUE;
        SudekiMpLogFormat(
            "split_screen_render event=player_two_native_camera "
            "phase=ready player_one_mode=%d player_two_mode=%d "
            "target_slot=0x%08lx target=0x%08lx actor_generation=%lu "
            "generation=%u "
            "p2_right_stick=disabled_native_ready_manual_fallback_only "
            "policy=native_exploration_collision_matrix_input_broadcast_suppressed_manual_orbit_disabled\r\n",
            player_one_mode,
            player_two_mode,
            (unsigned long)(uintptr_t)player_two_native_camera_target_slot,
            (unsigned long)(uintptr_t)player_two_native_camera_target_actor,
            (unsigned long)player_two_native_camera_actor_generation,
            (unsigned int)generation);
    }
    return player_two_native_camera_ready;
}

static BOOL update_player_two_render_state(void) {
    float first_position[3];
    float second_position[3];
    float camera_target[3];
    float matrix[16];
    float target_delta[3] = {0.0f, 0.0f, 0.0f};
    BOOL should_use_first_person;
    BOOL player_one_first_person = FALSE;
    BOOL combat_phase;
    BOOL skill_phase;
    BOOL settled_temporary;
    BOOL native_temporary_ready = FALSE;
    unsigned int temporary_policy;
    int player_one_camera_mode = -1;
    int rebase_phase;
    uint16_t *generation;

    if (!readable_memory(player_one_render_state, 0xdcu) ||
        !readable_memory(player_two_render_state, 0xdcu)) {
        if (player_two_native_camera_stage !=
                SUDEKIMP_NATIVE_CAMERA_STAGE_IDLE) {
            request_native_camera_manual_recreation(
                "manual_fallback_render_state_unreadable");
        }
        return FALSE;
    }
    if (!native_camera_resident_health_valid()) {
        return FALSE;
    }
    if (!render_state_camera_matrix_valid(player_one_render_state)) {
        if (player_two_native_camera_stage !=
                SUDEKIMP_NATIVE_CAMERA_STAGE_IDLE) {
            request_native_camera_manual_recreation(
                "manual_fallback_player_one_matrix_invalid");
        }
        return FALSE;
    }
    if (!character_position(player_one_character, first_position) ||
        !character_position(player_two_character, second_position)) {
        if (player_two_native_camera_stage !=
                SUDEKIMP_NATIVE_CAMERA_STAGE_IDLE) {
            request_native_camera_manual_recreation(
                "manual_fallback_actor_position_unreadable");
        }
        return FALSE;
    }
    settled_temporary = settled_temporary_zone_active();
    if (settled_temporary) {
        player_one_camera_mode = current_player_one_camera_mode();
        if (player_one_camera_mode == CAMERA_MODE_EXPLORATION) {
            native_temporary_ready = player_two_native_camera_is_ready();
            if (!native_temporary_ready) {
                (void)initialize_player_two_native_camera();
                native_temporary_ready = player_two_native_camera_is_ready();
            }
        } else {
            if (player_two_native_camera_ready) {
                arm_native_camera_recovery_window();
            }
            player_two_native_camera_ready = FALSE;
        }
        temporary_policy = SudekiMpSplitScreenTemporaryCameraPolicy(
            TRUE,
            player_one_camera_mode,
            native_temporary_ready);
        set_player_two_temporary_camera_policy(
            temporary_policy,
            player_one_camera_mode,
            temporary_policy == SUDEKIMP_TEMP_CAMERA_NATIVE_PLAYER_TWO ?
                "exact_native_exploration_ready" :
                (player_one_camera_mode == CAMERA_MODE_EXPLORATION ?
                    "native_player_two_not_ready" :
                    "fixed_transitional_or_unknown_player_one_mode"));
        if (temporary_policy ==
                SUDEKIMP_TEMP_CAMERA_NATIVE_PLAYER_TWO) {
            memcpy(
                player_two_camera_last_target,
                second_position,
                sizeof(player_two_camera_last_target));
            player_two_camera_transform_initialized = TRUE;
            player_two_camera_pitch_offset = 0.0f;
            player_two_first_person_camera_active = FALSE;
            player_two_forced_third_person_active = FALSE;
            player_two_camera_input_last_tick = GetTickCount();
            return TRUE;
        }

        /* Fixed, transitional, and unknown TEMP cameras remain at the exact
         * authored eye.  The frame scheduler presents only the native Player 1
         * frame at full width while this state is active, so this copy is a
         * warm, safe fallback rather than a duplicated split viewport. */
        memcpy(
            (uint8_t *)player_two_render_state + 0x90u,
            (uint8_t *)player_one_render_state + 0x90u,
            sizeof(matrix));
        memcpy(
            (uint8_t *)player_two_render_state + 0xd0u,
            (uint8_t *)player_one_render_state + 0xd0u,
            sizeof(float) * 3u);
        memcpy(
            player_two_camera_last_target,
            second_position,
            sizeof(player_two_camera_last_target));
        player_two_camera_transform_initialized = TRUE;
        player_two_camera_pitch_offset = 0.0f;
        player_two_first_person_camera_active = FALSE;
        player_two_forced_third_person_active = FALSE;
        player_two_camera_input_last_tick = GetTickCount();
        generation = (uint16_t *)(
            (uint8_t *)player_two_render_state + 0x2cu);
        ++*generation;
        if (player_two_native_camera_bound &&
            !player_two_native_camera_ready) {
            (void)SudekiMpSplitScreenObserveNativeCameraGeneration(
                &player_two_native_camera_start_generation,
                *generation,
                TRUE);
        }
        return TRUE;
    }
    if (player_two_temporary_camera_policy !=
            SUDEKIMP_TEMP_CAMERA_OUTSIDE) {
        unsigned int previous_temporary_policy =
            player_two_temporary_camera_policy;

        set_player_two_temporary_camera_policy(
            SUDEKIMP_TEMP_CAMERA_OUTSIDE,
            current_player_one_camera_mode(),
            "settled_temporary_zone_ended");
        if (previous_temporary_policy ==
                SUDEKIMP_TEMP_CAMERA_SHARED_FULL_WIDTH) {
            reset_player_two_controller_camera();
        }
        SudekiMpLogWrite(
            "split_screen_render event=player_two_camera "
            "phase=temporary_camera_exit "
            "policy=rebase_from_current_native_outdoor_camera\r\n");
    }
    if (player_two_native_camera_is_ready()) {
        if (!apply_player_two_native_camera_input()) {
            request_native_camera_manual_recreation(
                "native_right_stick_event_rejected");
            return FALSE;
        }
        memcpy(
            player_two_camera_last_target,
            second_position,
            sizeof(player_two_camera_last_target));
        player_two_camera_transform_initialized = TRUE;
        player_two_camera_pitch_offset = 0.0f;
        player_two_first_person_camera_active = FALSE;
        player_two_forced_third_person_active = FALSE;
        player_two_camera_input_last_tick = GetTickCount();
        return TRUE;
    }
    should_use_first_person =
        player_two_should_use_camera_first_person();
    if (readable_memory(player_one_character, 0x94u)) {
        uint8_t *arbiter = *(uint8_t **)(
            (uint8_t *)player_one_character + 0x90u
        );
        player_one_first_person = readable_memory(arbiter, 0x54u) &&
            (*(uint32_t *)(arbiter + 0x50u) &
             RANGED_FIRST_PERSON_ARBITER_FLAG) != 0u;
    }
    skill_phase = player_skill_render_states[0] != NULL ||
        current_spirit_presentation_state() != 0;
    combat_phase = active_group_in_combat() || player_one_first_person ||
        skill_phase;
    /* A skill camera is a caster-viewport presentation phase, not a new
     * observer orbit.  Keep Tal/Buki on the already-established combat
     * profile so the first authored activation frames cannot rotate or
     * rebase the companion camera. */
    rebase_phase = combat_phase ? 2 : 1;
    log_player_two_camera_phase(
        player_one_first_person,
        combat_phase,
        skill_phase
    );
    memcpy(camera_target, second_position, sizeof(camera_target));
    /* Keep the preserved third-person orbit on one stable target across a
     * perspective toggle.  First person uses its separate eye coordinate
     * below; changing this target would lower the saved orbit by one unit and
     * make an asynchronous reset restore the wrong height. */
    camera_target[1] += 1.0f;
    if (!second_player_controller_camera_enabled ||
        !player_two_camera_transform_initialized ||
        !player_two_native_matrix_valid()) {
        if (!render_state_camera_matrix_valid(player_one_render_state)) {
            if (player_two_native_camera_stage !=
                    SUDEKIMP_NATIVE_CAMERA_STAGE_IDLE) {
                request_native_camera_manual_recreation(
                    "manual_fallback_player_one_matrix_invalid");
            }
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
        if (second_player_controller_camera_enabled) {
            memcpy(
                player_two_camera_last_target,
                camera_target,
                sizeof(player_two_camera_last_target)
            );
            player_two_camera_input_last_tick = GetTickCount();
        }
    } else {
        memcpy(
            matrix,
            (uint8_t *)player_two_render_state + 0x90u,
            sizeof(matrix)
        );
        target_delta[0] = camera_target[0] -
            player_two_camera_last_target[0];
        target_delta[1] = camera_target[1] -
            player_two_camera_last_target[1];
        target_delta[2] = camera_target[2] -
            player_two_camera_last_target[2];
        if (player_two_first_person_camera_active) {
            player_two_third_person_matrix[12] += target_delta[0];
            player_two_third_person_matrix[13] += target_delta[1];
            player_two_third_person_matrix[14] += target_delta[2];
        } else {
            matrix[12] += target_delta[0];
            matrix[13] += target_delta[1];
            matrix[14] += target_delta[2];
        }
        memcpy(
            player_two_camera_last_target,
            camera_target,
            sizeof(player_two_camera_last_target)
        );
    }
    if (player_one_first_person && !should_use_first_person) {
        if (!player_two_forced_third_person_active ||
            !second_player_controller_camera_enabled ||
            player_two_rebased_phase != rebase_phase) {
            float forward_x = matrix[8];
            float forward_z = matrix[10];
            float horizontal_length = sqrtf(
                forward_x * forward_x + forward_z * forward_z
            );
            float camera_distance = combat_phase ?
                player_two_combat_camera_distance :
                player_two_exploration_camera_distance;
            float camera_height = combat_phase ?
                player_two_combat_camera_height :
                player_two_exploration_camera_height;

            if (!isfinite(horizontal_length) ||
                horizontal_length <= 0.0001f) {
                return FALSE;
            }
            forward_x /= horizontal_length;
            forward_z /= horizontal_length;
            matrix[12] = camera_target[0] -
                forward_x * camera_distance;
            matrix[13] = camera_target[1] + camera_height;
            matrix[14] = camera_target[2] -
                forward_z * camera_distance;
            player_two_forced_third_person_active = TRUE;
            player_two_rebased_phase = rebase_phase;
            SudekiMpLogFormat(
                "split_screen_render event=player_two_camera phase=third_person_rebase reason=player_one_native_first_person profile=%s distance_bits=0x%08lx height_bits=0x%08lx policy=preserve_native_phase_profile_and_keep_observer_melee_body_in_vanilla_style_orbit\r\n",
                combat_phase ? "combat" : "exploration",
                (unsigned long)float_bits(camera_distance),
                (unsigned long)float_bits(camera_height)
            );
        }
    } else {
        player_two_forced_third_person_active = FALSE;
    }
    if (should_use_first_person) {
        float eye[3] = {
            second_position[0],
            second_position[1] + player_two_first_person_eye_height,
            second_position[2]
        };

        if (!player_two_first_person_camera_active) {
            memcpy(
                player_two_third_person_matrix,
                matrix,
                sizeof(player_two_third_person_matrix)
            );
            player_two_first_person_camera_active = TRUE;
            SudekiMpLogFormat(
                "split_screen_render event=player_two_first_person_camera phase=enter character=0x%08lx eye_height_bits=0x%08lx policy=viewport_two_only_preserve_third_person_matrix_for_exit\r\n",
                (unsigned long)(uintptr_t)player_two_character,
                (unsigned long)float_bits(
                    player_two_first_person_eye_height
                )
            );
        }
        if (!SudekiMpFirstPersonCameraTransform(
                matrix, eye, 0.0f, 0.0f)) {
            return FALSE;
        }
    } else if (player_two_first_person_camera_active) {
        memcpy(
            matrix,
            player_two_third_person_matrix,
            sizeof(matrix)
        );
        player_two_first_person_camera_active = FALSE;
        SudekiMpLogFormat(
            "split_screen_render event=player_two_first_person_camera phase=exit character=0x%08lx policy=restore_preserved_third_person_orbit\r\n",
            (unsigned long)(uintptr_t)player_two_character
        );
    }
    if (second_player_controller_camera_enabled &&
        !apply_player_two_controller_camera_input(
            matrix,
            camera_target,
            should_use_first_person)) {
        return FALSE;
    }
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
    /* A valid Camera-2 basis belongs to the seat even when that seat has no
     * active orbit input. The orbit flag controls mutation of this matrix;
     * it does not control whether movement may read the resulting view. */
    player_two_camera_transform_initialized = TRUE;
    generation = (uint16_t *)((uint8_t *)player_two_render_state + 0x2cu);
    ++*generation;
    if (player_two_native_camera_bound &&
        !player_two_native_camera_ready) {
        (void)SudekiMpSplitScreenObserveNativeCameraGeneration(
            &player_two_native_camera_start_generation,
            *generation,
            TRUE);
    }
    (void)initialize_player_two_native_camera();
    return TRUE;
}

BOOL SudekiMpTransformSeatMovement(
    unsigned int seat_index,
    const void *character,
    const float local_direction[3],
    float world_direction[3]
) {
    float matrix[16];
    void *render_state;

    if (local_direction == NULL || world_direction == NULL ||
        character == NULL) {
        return FALSE;
    }
    if (fixed_three_assignment_selected()) {
        if (seat_index < FIXED_THREE_COMPANION_FIRST_SEAT ||
            seat_index > FIXED_THREE_COMPANION_LAST_SEAT ||
            !fixed_three_runtime.camera_transform_initialized[seat_index] ||
            !fixed_three_seat_view_exact(seat_index, character)) {
            return FALSE;
        }
        render_state = fixed_three_runtime.render_states[seat_index];
    } else {
        if (seat_index != 1u || character != player_two_character ||
            player_two_camera == NULL ||
            !player_two_camera_transform_initialized ||
            !readable_memory(player_two_render_state, 0xd0u) ||
            !render_state_camera_matrix_valid(player_two_render_state)) {
            return FALSE;
        }
        render_state = player_two_render_state;
    }
    memcpy(
        matrix,
        (uint8_t *)render_state + 0x90u,
        sizeof(matrix)
    );
    return SudekiMpCameraTransformHorizontalDirection(
        matrix, local_direction, world_direction
    );
}

BOOL SudekiMpTransformPlayerTwoMovement(
    const float local_direction[3],
    float world_direction[3]
) {
    return SudekiMpTransformSeatMovement(
        1u,
        player_two_character,
        local_direction,
        world_direction
    );
}

__attribute__((naked, noinline, used))
static void call_position_set_forward(
    void *position __attribute__((unused)),
    const float direction[3] __attribute__((unused))
) {
    __asm__ volatile(
        "pushl %esi\n\t"
        "movl 8(%esp), %esi\n\t"
        "movl 12(%esp), %ecx\n\t"
        "call *_position_set_forward_function\n\t"
        "popl %esi\n\t"
        "ret\n\t"
    );
}

BOOL SudekiMpAlignSeatFacingToCamera(
    unsigned int seat_index,
    void *character
) {
    static const float local_forward[3] = {0.0f, 0.0f, 1.0f};
    uint8_t *position;
    float world_forward[3];

    if ((seat_index < FIXED_THREE_COMPANION_FIRST_SEAT ||
         seat_index > FIXED_THREE_COMPANION_LAST_SEAT) ||
        (!fixed_three_assignment_selected() && seat_index != 1u) ||
        !second_player_controller_camera_enabled ||
        position_set_forward_function == NULL ||
        character == NULL ||
        (fixed_three_assignment_selected() ?
            character != fixed_three_runtime.actors[seat_index] :
            character != player_two_character) ||
        !readable_memory(character, 0x48u) ||
        !SudekiMpTransformSeatMovement(
            seat_index,
            character,
            local_forward,
            world_forward)) {
        return FALSE;
    }
    position = *(uint8_t **)((uint8_t *)character + 0x44u);
    if (!readable_memory(position, 0xbcu)) {
        return FALSE;
    }
    call_position_set_forward(position, world_forward);
    if (fixed_three_assignment_selected() ?
            !fixed_three_facing_logged[seat_index] :
            !player_two_facing_logged) {
        if (fixed_three_assignment_selected()) {
            fixed_three_facing_logged[seat_index] = TRUE;
        } else {
            player_two_facing_logged = TRUE;
        }
        SudekiMpLogFormat(
            "split_screen_render event=seat_camera_facing player=%u "
            "phase=active character=0x%08lx position=0x%08lx "
            "policy=native_position_orientation_commit_from_owned_camera_forward\r\n",
            seat_index + 1u,
            (unsigned long)(uintptr_t)character,
            (unsigned long)(uintptr_t)position
        );
    }
    return TRUE;
}

BOOL SudekiMpAlignPlayerTwoFacingToCamera(void *character) {
    return SudekiMpAlignSeatFacingToCamera(1u, character);
}

void SudekiMpSplitScreenBeginSkillCameraCall(void *caster) {
    pending_skill_camera_caster = caster;
}

void SudekiMpSplitScreenEndSkillCameraCall(void) {
    pending_skill_camera_caster = NULL;
}

void SudekiMpSplitScreenClearSkillCamera(void *caster, const char *reason) {
    int player = caster == player_one_character ? 0 :
        (caster == player_two_character ? 1 : -1);

    if (player < 0 || player_skill_render_states[player] == NULL) {
        return;
    }
    player_skill_cameras[player] = NULL;
    player_skill_render_states[player] = NULL;
    SudekiMpCombatContextSetView(
        (unsigned int)player,
        player == 0 ? player_one_camera : player_two_camera,
        player == 0 ? player_one_render_state : player_two_render_state
    );
    SudekiMpLogFormat(
        "realtime_skill_combat event=skill_camera_cleanup player=%d caster=0x%08lx reason=%s policy=restore_viewport_on_cancel_interrupt_or_end\r\n",
        player + 1,
        (unsigned long)(uintptr_t)caster,
        reason == NULL ? "unspecified" : reason
    );
}

static BOOL SUDEKIMP_THISCALL route_skill_render_camera(
    void *manager,
    const char *name
) {
    CameraManagerSetRenderCameraFunction original =
        (CameraManagerSetRenderCameraFunction)set_render_camera_hook.trampoline;
    void *requested_camera;
    void *render_state;
    void *camera_caster;
    BOOL inferred_spirit_caster = FALSE;
    BOOL inferred_native_skill_caster = FALSE;
    BOOL player_one_using_skill;
    BOOL player_two_using_skill;
    void *caller = __builtin_return_address(0);
    void *native_camera_before;
    unsigned int request_sequence;
    unsigned long caller_rva;
    int player;

    if (fixed_three_assignment_selected() ||
        !skill_camera_routing_enabled ||
        manager != second_player_camera_manager || name == NULL) {
        return original(manager, name);
    }
    request_sequence = ++skill_camera_request_sequence;
    caller_rva = game_base != NULL &&
        (uintptr_t)caller >= (uintptr_t)game_base ?
        (unsigned long)((uintptr_t)caller - (uintptr_t)game_base) :
        0xfffffffful;
    native_camera_before = current_render_camera(manager);
    requested_camera = camera_manager_get_camera(manager, name);
    camera_caster = pending_skill_camera_caster;
    if (camera_caster == NULL &&
        spirit_strike_viewport_effect_isolation_enabled &&
        current_spirit_presentation_state() != 0 &&
        player_one_character != NULL) {
        camera_caster = player_one_character;
        inferred_spirit_caster = TRUE;
    }
    player_one_using_skill = character_is_using_skill(player_one_character);
    player_two_using_skill = character_is_using_skill(player_two_character);
    SudekiMpLogFormat(
        "skill_camera_trace event=set_render_camera_request request=%u caller=0x%08lx caller_rva=0x%08lx manager=0x%08lx name=%s native_camera_before=0x%08lx requested_camera=0x%08lx pending_caster=0x%08lx p1_character=0x%08lx p2_character=0x%08lx p1_using_skill=%u p2_using_skill=%u p1_routed_state=0x%08lx p2_routed_state=0x%08lx policy=observation_only_exact_native_trigger\r\n",
        request_sequence,
        (unsigned long)(uintptr_t)caller,
        caller_rva,
        (unsigned long)(uintptr_t)manager,
        name,
        (unsigned long)(uintptr_t)native_camera_before,
        (unsigned long)(uintptr_t)requested_camera,
        (unsigned long)(uintptr_t)pending_skill_camera_caster,
        (unsigned long)(uintptr_t)player_one_character,
        (unsigned long)(uintptr_t)player_two_character,
        player_one_using_skill ? 1u : 0u,
        player_two_using_skill ? 1u : 0u,
        (unsigned long)(uintptr_t)player_skill_render_states[0],
        (unsigned long)(uintptr_t)player_skill_render_states[1]
    );
    if (camera_caster == NULL && active_group_in_combat() &&
        player_one_using_skill != player_two_using_skill) {
        camera_caster = player_one_using_skill ?
            player_one_character : player_two_character;
        inferred_native_skill_caster = TRUE;
    }
    if (camera_caster == NULL && requested_camera == player_one_camera) {
        if (player_skill_render_states[0] != NULL &&
            player_skill_render_states[1] == NULL) {
            camera_caster = player_one_character;
            inferred_native_skill_caster = TRUE;
        } else if (player_skill_render_states[1] != NULL &&
                   player_skill_render_states[0] == NULL) {
            camera_caster = player_two_character;
            inferred_native_skill_caster = TRUE;
        }
    }
    if (camera_caster == NULL) {
        if (player_one_using_skill && player_two_using_skill) {
            SudekiMpLogFormat(
                "realtime_skill_combat event=skill_camera_fallback name=%s reason=concurrent_native_skill_ownership_ambiguous policy=leave_native_camera_unchanged_until_per_player_camera_stacks_exist\r\n",
                name
            );
        }
        SudekiMpLogFormat(
            "skill_camera_trace event=set_render_camera_outcome request=%u outcome=native_original reason=no_unambiguous_caster name=%s\r\n",
            request_sequence,
            name
        );
        return original(manager, name);
    }
    player = camera_caster == player_one_character ? 0 :
        (camera_caster == player_two_character ? 1 : -1);
    if (player < 0 || !readable_memory(requested_camera, 0x38u)) {
        SudekiMpLogFormat(
            "realtime_skill_combat event=skill_camera_fallback caster=0x%08lx name=%s reason=%s\r\n",
            (unsigned long)(uintptr_t)camera_caster,
            name,
            player < 0 ? "caster_not_assigned_to_viewport" :
                "requested_camera_unavailable"
        );
        SudekiMpLogFormat(
            "skill_camera_trace event=set_render_camera_outcome request=%u outcome=native_original reason=%s name=%s\r\n",
            request_sequence,
            player < 0 ? "caster_not_assigned_to_viewport" :
                "requested_camera_unavailable",
            name
        );
        return original(manager, name);
    }
    if (requested_camera == player_one_camera) {
        skill_camera_history_tail_until = GetTickCount() + 1000u;
        SudekiMpSplitScreenClearSkillCamera(
            camera_caster,
            "native_normal_camera_restore"
        );
        SudekiMpLogFormat(
            "realtime_skill_combat event=skill_camera_restore player=%d caster=0x%08lx camera=0x%08lx source=%s policy=viewport_only_global_camera_unchanged\r\n",
            player + 1,
            (unsigned long)(uintptr_t)camera_caster,
            (unsigned long)(uintptr_t)requested_camera,
            inferred_spirit_caster ? "active_spirit_manager" :
                (inferred_native_skill_caster ?
                    "native_is_using_skill_owner" :
                    "skill_script_context")
        );
        SudekiMpLogFormat(
            "skill_camera_trace event=set_render_camera_outcome request=%u outcome=viewport_restore player=%d caster=0x%08lx camera=0x%08lx name=%s global_camera_unchanged=1\r\n",
            request_sequence,
            player + 1,
            (unsigned long)(uintptr_t)camera_caster,
            (unsigned long)(uintptr_t)requested_camera,
            name
        );
        SudekiMpLogFormat(
            "skill_camera_trace event=history_restore_tail request=%u player=%d duration_ms=1000 reason=default_camera_temporal_history_continues_after_routed_state_cleanup policy=player_two_private_history_until_restore_blend_drains\r\n",
            request_sequence,
            player + 1
        );
        return TRUE;
    }
    render_state = *(void **)((uint8_t *)requested_camera + 0x34u);
    if (!readable_memory(render_state, 0xdcu)) {
        SudekiMpLogFormat(
            "realtime_skill_combat event=skill_camera_fallback player=%d caster=0x%08lx camera=0x%08lx reason=render_state_unavailable\r\n",
            player + 1,
            (unsigned long)(uintptr_t)camera_caster,
            (unsigned long)(uintptr_t)requested_camera
        );
        SudekiMpLogFormat(
            "skill_camera_trace event=set_render_camera_outcome request=%u outcome=native_original reason=render_state_unavailable name=%s\r\n",
            request_sequence,
            name
        );
        return original(manager, name);
    }
    player_skill_cameras[player] = requested_camera;
    player_skill_render_states[player] = render_state;
    SudekiMpCombatContextSetView(
        (unsigned int)player,
        requested_camera,
        render_state
    );
    SudekiMpLogFormat(
        "realtime_skill_combat event=skill_camera_route player=%d caster=0x%08lx camera=0x%08lx render_state=0x%08lx name=%s source=%s policy=caster_viewport_only_global_camera_unchanged\r\n",
        player + 1,
        (unsigned long)(uintptr_t)camera_caster,
        (unsigned long)(uintptr_t)requested_camera,
        (unsigned long)(uintptr_t)render_state,
        name,
        inferred_spirit_caster ? "active_spirit_manager" :
            (inferred_native_skill_caster ?
                "native_is_using_skill_owner" :
                "skill_script_context")
    );
    SudekiMpLogFormat(
        "skill_camera_trace event=set_render_camera_outcome request=%u outcome=viewport_route player=%d caster=0x%08lx camera=0x%08lx render_state=0x%08lx name=%s source=%s global_camera_unchanged=1\r\n",
        request_sequence,
        player + 1,
        (unsigned long)(uintptr_t)camera_caster,
        (unsigned long)(uintptr_t)requested_camera,
        (unsigned long)(uintptr_t)render_state,
        name,
        inferred_spirit_caster ? "active_spirit_manager" :
            (inferred_native_skill_caster ?
                "native_is_using_skill_owner" :
                "skill_script_context")
    );
    return TRUE;
}

static void *model_animation_method(
    void *model_interface,
    size_t offset
) {
    void *vtable;
    void *method;

    if (!readable_memory(model_interface, sizeof(void *))) {
        return NULL;
    }
    vtable = *(void **)model_interface;
    if (!readable_memory((uint8_t *)vtable + offset, sizeof(void *))) {
        return NULL;
    }
    method = *(void **)((uint8_t *)vtable + offset);
    return executable_memory(method) ? method : NULL;
}

static BOOL ranged_state_details_renderer(
    void *wrapper,
    void **renderer_result,
    ModelAnimationLookupFunction *lookup_result,
    const char **reason_result
) {
    void *renderer;
    void *lookup;

    *renderer_result = NULL;
    *lookup_result = NULL;
    *reason_result = "unknown";
    if (game_base == NULL) {
        *reason_result = "game_base_unavailable";
        return FALSE;
    }
    if (!readable_memory(wrapper, 0x14u)) {
        *reason_result = "wrapper_unreadable";
        return FALSE;
    }
    renderer = *(void **)((uint8_t *)wrapper + 0x10u);
    *renderer_result = renderer;
    if (!readable_memory(renderer, sizeof(void *))) {
        *reason_result = "renderer_unreadable";
        return FALSE;
    }
    if (*(void **)renderer != game_base + RVA_ANIMATION_RENDERER_VTABLE) {
        *reason_result = "renderer_vtable_mismatch";
        return FALSE;
    }
    lookup = model_animation_method(renderer, 0x40u);
    if (lookup != game_base + RVA_ANIMATION_RENDERER_LOOKUP) {
        *reason_result = "renderer_lookup_identity_mismatch";
        return FALSE;
    }
    *lookup_result = (ModelAnimationLookupFunction)lookup;
    *reason_result = "resolved";
    return TRUE;
}

static int resolve_ranged_state_details_handle(
    void *renderer,
    ModelAnimationLookupFunction lookup,
    uint32_t handle
) {
    if (handle == 0u || handle == 0x0007ffffu) {
        return -2;
    }
    /* Handles are opaque 32-bit values; the sign bit is not an invalidity. */
    return lookup(renderer, (int)(int32_t)handle);
}

static void reset_ranged_world_compositor(const char *reason) {
    if (ranged_world_compositor_state.owned) {
        SudekiMpLogFormat(
            "split_screen_render event=ranged_world_compositor phase=reset reason=%s character=0x%08lx component=0x%08lx first_person_wrapper=0x%08lx world_wrapper=0x%08lx world_renderer=0x%08lx previous=%s moving=%s action_playing=%s action_source_id=0x%02x action_target_id=0x%02x action_selector=%d policy=clear_ownership_guard_without_animation_write\r\n",
            reason == NULL ? "unspecified" : reason,
            (unsigned long)(uintptr_t)
                ranged_world_compositor_state.character,
            (unsigned long)(uintptr_t)
                ranged_world_compositor_state.component,
            (unsigned long)(uintptr_t)
                ranged_world_compositor_state.first_person_wrapper,
            (unsigned long)(uintptr_t)
                ranged_world_compositor_state.world_wrapper,
            (unsigned long)(uintptr_t)
                ranged_world_compositor_state.world_renderer,
            ranged_world_compositor_state.applied ? "applied" : "rejected",
            ranged_world_compositor_state.moving ? "true" : "false",
            ranged_world_compositor_state.action_playing ? "true" : "false",
            ranged_world_compositor_state.action_source_id,
            ranged_world_compositor_state.action_target_id,
            ranged_world_compositor_state.action_selector
        );
    }
    ZeroMemory(
        &ranged_world_compositor_state,
        sizeof(ranged_world_compositor_state)
    );
}

static void claim_ranged_world_compositor(
    void *character,
    void *component,
    void *first_person_wrapper,
    void *world_wrapper
) {
    if (ranged_world_compositor_state.owned &&
        (ranged_world_compositor_state.character != character ||
         ranged_world_compositor_state.component != component ||
         ranged_world_compositor_state.first_person_wrapper !=
            first_person_wrapper ||
         ranged_world_compositor_state.world_wrapper != world_wrapper)) {
        reset_ranged_world_compositor("ownership_changed");
    }
    if (!ranged_world_compositor_state.owned) {
        ranged_world_compositor_state.owned = TRUE;
        ranged_world_compositor_state.character = character;
        ranged_world_compositor_state.component = component;
        ranged_world_compositor_state.first_person_wrapper =
            first_person_wrapper;
        ranged_world_compositor_state.world_wrapper = world_wrapper;
    }
}

static BOOL reject_ranged_world_compositor(const char *reason) {
    if (ranged_world_compositor_state.last_rejection != reason ||
        ranged_world_compositor_state.applied) {
        SudekiMpLogFormat(
            "split_screen_render event=ranged_world_compositor phase=rejected reason=%s character=0x%08lx component=0x%08lx first_person_wrapper=0x%08lx world_wrapper=0x%08lx world_renderer=0x%08lx write_attempted=%s policy=%s\r\n",
            reason,
            (unsigned long)(uintptr_t)
                ranged_world_compositor_state.character,
            (unsigned long)(uintptr_t)
                ranged_world_compositor_state.component,
            (unsigned long)(uintptr_t)
                ranged_world_compositor_state.first_person_wrapper,
            (unsigned long)(uintptr_t)
                ranged_world_compositor_state.world_wrapper,
            (unsigned long)(uintptr_t)
                ranged_world_compositor_state.world_renderer,
            ranged_world_compositor_state.write_attempted ? "true" : "false",
            ranged_world_compositor_state.write_attempted ?
                "fallback_stable_mirror_no_retry_until_ownership_reset" :
                "retry_same_ownership_without_animation_write"
        );
    }
    ranged_world_compositor_state.applied = FALSE;
    ranged_world_compositor_state.last_rejection = reason;
    return FALSE;
}

/*
 * The detached Ailish world renderer is natively advanced by Sudeki, but it
 * has no second gameplay animation-controller owner.  Compose only the
 * confirmed native world clips at presentation level: hard-cut the base
 * locomotion state when movement changes and play the confirmed standard or
 * shock fire action on channel 4.  The first-person-only C1/C2/C3 reload
 * clips are intentionally ignored.  Never tick the renderer or call the
 * gameplay controller.
 */
static BOOL ranged_world_animation_methods(
    void *renderer,
    SudekiMpRangedWorldAnimationMethods *methods,
    const char **reason_result
) {
    void *method;

    ZeroMemory(methods, sizeof(*methods));
    *reason_result = "animation_method_identity_mismatch";
    method = model_animation_method(renderer, 0xf8u);
    if (method != game_base + RVA_ANIMATION_RENDERER_COUNT) {
        return FALSE;
    }
    methods->count = (ModelAnimationCountFunction)method;
    method = model_animation_method(renderer, 0xfcu);
    if (method != game_base + RVA_ANIMATION_RENDERER_SELECTOR_SET) {
        return FALSE;
    }
    methods->set_selector = (ModelAnimationSelectorSetFunction)method;
    method = model_animation_method(renderer, 0x100u);
    if (method != game_base + RVA_ANIMATION_RENDERER_SELECTOR_GET) {
        return FALSE;
    }
    methods->get_selector = (ModelAnimationSelectorGetFunction)method;
    method = model_animation_method(renderer, 0x104u);
    if (method != game_base + RVA_ANIMATION_RENDERER_RATE_SET) {
        return FALSE;
    }
    methods->set_rate = (ModelAnimationValueSetFunction)method;
    method = model_animation_method(renderer, 0x108u);
    if (method != game_base + RVA_ANIMATION_RENDERER_RATE_GET) {
        return FALSE;
    }
    methods->get_rate = (ModelAnimationValueGetFunction)method;
    method = model_animation_method(renderer, 0x10cu);
    if (method != game_base + RVA_ANIMATION_RENDERER_TIME_SET) {
        return FALSE;
    }
    methods->set_time = (ModelAnimationTimeSetFunction)method;
    method = model_animation_method(renderer, 0x110u);
    if (method != game_base + RVA_ANIMATION_RENDERER_TIME_GET) {
        return FALSE;
    }
    methods->get_time = (ModelAnimationValueGetFunction)method;
    method = model_animation_method(renderer, 0x114u);
    if (method != game_base + RVA_ANIMATION_RENDERER_STATE_SET) {
        return FALSE;
    }
    methods->set_state = (ModelAnimationStateSetFunction)method;
    method = model_animation_method(renderer, 0x118u);
    if (method != game_base + RVA_ANIMATION_RENDERER_STATE_GET) {
        return FALSE;
    }
    methods->get_state = (ModelAnimationStateGetFunction)method;
    method = model_animation_method(renderer, 0x144u);
    if (method != game_base + RVA_ANIMATION_RENDERER_BLEND_SET) {
        return FALSE;
    }
    methods->set_blend = (ModelAnimationBlendSetFunction)method;
    method = model_animation_method(renderer, 0x148u);
    if (method != game_base + RVA_ANIMATION_RENDERER_BLEND_GET) {
        return FALSE;
    }
    methods->get_blend = (ModelAnimationBlendGetFunction)method;
    *reason_result = "resolved";
    return TRUE;
}

/*
 * A non-caster can now translate through the native absolute-delta collision
 * path while a Spirit Strike owns Sudeki's global presentation state.  Keep
 * this probe read-only: distinguish a live character transform from a stale
 * model root or a stopped renderer clock before attempting animation
 * virtualization.
 */
static void trace_spirit_player_two_presentation(void) {
    uint8_t *character;
    uint8_t *position;
    uint8_t *wrapper;
    uint8_t *render_object;
    uint8_t *renderer;
    ModelAnimationSelectorGetFunction get_selector;
    ModelAnimationValueGetFunction get_rate;
    ModelAnimationValueGetFunction get_time;
    ModelAnimationStateGetFunction get_state;
    const float *model_matrix;
    const float *camera_matrix;
    uint8_t *movement_controller;
    float movement_speed;
    BOOL spirit_active;
    BOOL position_changed = FALSE;
    DWORD now;
    int channel_count;
    int channel;

    spirit_active = current_spirit_presentation_state() != 0;
    character = (uint8_t *)player_two_character;
    position = readable_memory(character, 0x48u) ?
        *(uint8_t **)(character + 0x44u) : NULL;
    movement_controller = readable_memory(character, 0x84u) ?
        *(uint8_t **)(character + 0x80u) : NULL;
    movement_speed = readable_memory(movement_controller, 0x28u) ?
        *(float *)(movement_controller + 0x24u) : 0.0f;
    if (readable_memory(position, 0x24u)) {
        float current_position[3] = {
            *(float *)(position + 0x18u),
            *(float *)(position + 0x1cu),
            *(float *)(position + 0x20u)
        };
        if (spirit_player_two_presentation_position_valid &&
            isfinite(current_position[0]) &&
            isfinite(current_position[1]) &&
            isfinite(current_position[2])) {
            float delta_x = current_position[0] -
                spirit_player_two_presentation_position[0];
            float delta_y = current_position[1] -
                spirit_player_two_presentation_position[1];
            float delta_z = current_position[2] -
                spirit_player_two_presentation_position[2];
            position_changed =
                delta_x * delta_x + delta_y * delta_y + delta_z * delta_z >
                0.000001f;
        }
        memcpy(
            spirit_player_two_presentation_position,
            current_position,
            sizeof(current_position)
        );
        spirit_player_two_presentation_position_valid = TRUE;
    } else {
        spirit_player_two_presentation_position_valid = FALSE;
    }
    if (!spirit_active &&
        (!isfinite(movement_speed) || movement_speed <= 0.1f) &&
        !position_changed) {
        spirit_player_two_presentation_last_trace_tick = 0u;
        return;
    }
    now = GetTickCount();
    if (spirit_player_two_presentation_last_trace_tick != 0u &&
        (DWORD)(now - spirit_player_two_presentation_last_trace_tick) < 200u) {
        return;
    }
    wrapper = readable_memory(position, 0xb8u) ?
        *(uint8_t **)(position + 0xb4u) : NULL;
    render_object = readable_memory(wrapper, 0x14u) ?
        *(uint8_t **)(wrapper + 0x08u) : NULL;
    renderer = readable_memory(wrapper, 0x14u) ?
        *(uint8_t **)(wrapper + 0x10u) : NULL;
    model_matrix = readable_memory(render_object, 0xd0u) ?
        (const float *)(render_object + 0x90u) : NULL;
    camera_matrix = readable_memory(player_two_render_state, 0xd0u) ?
        (const float *)((uint8_t *)player_two_render_state + 0x90u) : NULL;
    if (!readable_memory(position, 0x24u) ||
        !readable_memory(renderer, 0xa8u) ||
        *(void **)renderer != game_base + RVA_ANIMATION_RENDERER_VTABLE ||
        model_matrix == NULL || camera_matrix == NULL) {
        SudekiMpLogFormat(
            "split_screen_render event=spirit_player_two_presentation phase=rejected character=0x%08lx position=0x%08lx wrapper=0x%08lx render_object=0x%08lx renderer=0x%08lx policy=read_only_model_root_and_animation_clock_trace\r\n",
            (unsigned long)(uintptr_t)character,
            (unsigned long)(uintptr_t)position,
            (unsigned long)(uintptr_t)wrapper,
            (unsigned long)(uintptr_t)render_object,
            (unsigned long)(uintptr_t)renderer
        );
        spirit_player_two_presentation_last_trace_tick = now;
        return;
    }
    get_selector = (ModelAnimationSelectorGetFunction)
        model_animation_method(renderer, 0x100u);
    get_rate = (ModelAnimationValueGetFunction)
        model_animation_method(renderer, 0x108u);
    get_time = (ModelAnimationValueGetFunction)
        model_animation_method(renderer, 0x110u);
    get_state = (ModelAnimationStateGetFunction)
        model_animation_method(renderer, 0x118u);
    channel_count = *(int *)(renderer + 0xa0u);
    if (get_selector != (ModelAnimationSelectorGetFunction)(
            game_base + RVA_ANIMATION_RENDERER_SELECTOR_GET) ||
        get_rate != (ModelAnimationValueGetFunction)(
            game_base + RVA_ANIMATION_RENDERER_RATE_GET) ||
        get_time != (ModelAnimationValueGetFunction)(
            game_base + RVA_ANIMATION_RENDERER_TIME_GET) ||
        get_state != (ModelAnimationStateGetFunction)(
            game_base + RVA_ANIMATION_RENDERER_STATE_GET) ||
        channel_count <= 0 || channel_count > 32) {
        SudekiMpLogFormat(
            "split_screen_render event=spirit_player_two_presentation phase=rejected character=0x%08lx renderer=0x%08lx channel_count=%d reason=renderer_method_or_count_gate policy=read_only_model_root_and_animation_clock_trace\r\n",
            (unsigned long)(uintptr_t)character,
            (unsigned long)(uintptr_t)renderer,
            channel_count
        );
        spirit_player_two_presentation_last_trace_tick = now;
        return;
    }
    SudekiMpLogFormat(
        "split_screen_render event=spirit_player_two_presentation phase=sample scope=%s character=0x%08lx movement_speed_bits=0x%08lx position=%.5f,%.5f,%.5f model_root=%.5f,%.5f,%.5f camera_eye=%.5f,%.5f,%.5f channel_count=%d policy=read_only_model_root_and_animation_clock_trace\r\n",
        spirit_active ? "spirit" : "native_baseline",
        (unsigned long)(uintptr_t)character,
        (unsigned long)float_bits(movement_speed),
        *(float *)(position + 0x18u),
        *(float *)(position + 0x1cu),
        *(float *)(position + 0x20u),
        model_matrix[12], model_matrix[13], model_matrix[14],
        camera_matrix[12], camera_matrix[13], camera_matrix[14],
        channel_count
    );
    for (channel = 0; channel < channel_count && channel < 5; ++channel) {
        SudekiMpLogFormat(
            "split_screen_render event=spirit_player_two_presentation phase=channel scope=%s channel=%d selector=%d state=%u rate_bits=0x%08lx time_bits=0x%08lx policy=read_only_model_root_and_animation_clock_trace\r\n",
            spirit_active ? "spirit" : "native_baseline",
            channel,
            get_selector(renderer, channel, 0u),
            (unsigned int)get_state(renderer, channel, 0u),
            (unsigned long)float_bits(get_rate(renderer, channel, 0u)),
            (unsigned long)float_bits(get_time(renderer, channel, 0u))
        );
    }
    spirit_player_two_presentation_last_trace_tick = now;
}

static BOOL resolve_ranged_world_animation_selector(
    uint8_t *component,
    void *renderer,
    ModelAnimationLookupFunction lookup,
    unsigned int animation_id,
    int required_selector,
    int *selector_result,
    const char **reason_result
) {
    uint8_t *animation_table;
    uint8_t *details;
    uint32_t handle;
    BOOL selected_first_bank;
    int selector;

    *selector_result = -2;
    if (!readable_memory(component, 0x134u)) {
        *reason_result = "animation_component_unavailable";
        return FALSE;
    }
    animation_table = *(uint8_t **)(component + 0xdcu);
    if (!readable_memory(
            animation_table,
            0x14u + (animation_id + 1u) * sizeof(void *))) {
        *reason_result = "animation_table_unavailable";
        return FALSE;
    }
    details = *(uint8_t **)(
        animation_table + 0x14u + animation_id * sizeof(void *)
    );
    if (!readable_memory(details, 0x28u)) {
        *reason_result = "animation_details_unavailable";
        return FALSE;
    }
    selected_first_bank = (component[0x133u] & 2u) != 0u;
    memcpy(
        &handle,
        details + (selected_first_bank ? 0x14u : 0x20u),
        sizeof(handle)
    );
    selector = resolve_ranged_state_details_handle(renderer, lookup, handle);
    *selector_result = selector;
    if (selector != required_selector) {
        *reason_result = "animation_selector_exact_gate_failed";
        return FALSE;
    }
    *reason_result = "resolved";
    return TRUE;
}

static BOOL classify_ranged_world_movement(
    void *character,
    float *speed_result,
    BOOL *gate_result,
    BOOL *moving_result,
    const char **reason_result
) {
    uint8_t *character_bytes = (uint8_t *)character;
    uint8_t *arbiter;
    uint8_t *movement_state;
    float speed;

    *speed_result = 0.0f;
    *gate_result = FALSE;
    *moving_result = FALSE;
    if (!readable_memory(character_bytes, 0x94u)) {
        *reason_result = "movement_character_unavailable";
        return FALSE;
    }
    arbiter = *(uint8_t **)(character_bytes + 0x90u);
    if (!readable_memory(arbiter, 0x14u) ||
        *(void **)(arbiter + 0x10u) != character) {
        *reason_result = "movement_arbiter_owner_gate_failed";
        return FALSE;
    }
    movement_state = *(uint8_t **)(character_bytes + 0x80u);
    if (!readable_memory(movement_state, 0xc0u)) {
        *reason_result = "movement_state_unavailable";
        return FALSE;
    }
    memcpy(&speed, movement_state + 0x24u, sizeof(speed));
    if (!isfinite(speed)) {
        *reason_result = "movement_speed_not_finite";
        return FALSE;
    }
    speed = fabsf(speed);
    *gate_result = (movement_state[0xbeu] & 0x08u) != 0u;
    *speed_result = speed;
    *moving_result = *gate_result && speed > 0.0001f;
    *reason_result = "resolved";
    return TRUE;
}

static BOOL ranged_source_animation_present(
    uint8_t *component,
    unsigned int animation_id,
    BOOL *present_result,
    const char **reason_result
) {
    uint8_t *animation_state;
    unsigned int channel;

    *present_result = FALSE;
    if (!readable_memory(component, 0xfcu)) {
        *reason_result = "source_animation_component_unavailable";
        return FALSE;
    }
    animation_state = *(uint8_t **)(component + 0xf8u);
    if (!readable_memory(
            animation_state,
            RANGED_ANIMATION_CHANNEL_COUNT * 4u)) {
        *reason_result = "source_animation_state_unavailable";
        return FALSE;
    }
    for (channel = 0u;
         channel < RANGED_ANIMATION_CHANNEL_COUNT;
         ++channel) {
        if (animation_state[channel * 4u + 2u] == animation_id) {
            *present_result = TRUE;
            break;
        }
    }
    *reason_result = "resolved";
    return TRUE;
}

static void set_ranged_world_animation_channel(
    void *renderer,
    const SudekiMpRangedWorldAnimationMethods *methods,
    unsigned int submodels,
    int channel,
    int selector,
    int state,
    float rate
) {
    unsigned int submodel;

    for (submodel = 0u; submodel < submodels; ++submodel) {
        methods->set_selector(renderer, channel, submodel, selector);
        methods->set_state(renderer, channel, submodel, state);
        methods->set_time(renderer, channel, submodel, 0.0f, 0);
        methods->set_rate(renderer, channel, submodel, rate);
    }
}

static BOOL verify_ranged_world_animation_channel(
    void *renderer,
    const SudekiMpRangedWorldAnimationMethods *methods,
    unsigned int submodels,
    int channel,
    int selector,
    unsigned int state,
    float rate
) {
    unsigned int submodel;

    for (submodel = 0u; submodel < submodels; ++submodel) {
        float current_time = methods->get_time(
            renderer, channel, submodel
        );
        float current_rate = methods->get_rate(
            renderer, channel, submodel
        );

        if (methods->get_selector(renderer, channel, submodel) != selector ||
            methods->get_state(renderer, channel, submodel) != state ||
            !isfinite(current_time) || !isfinite(current_rate) ||
            fabsf(current_rate - rate) > 0.0001f) {
            return FALSE;
        }
    }
    return TRUE;
}

static void reset_spirit_player_two_melee_locomotion(const char *reason) {
    if (spirit_player_two_melee_locomotion_owned) {
        SudekiMpLogFormat(
            "split_screen_render event=spirit_player_two_melee_locomotion phase=reset reason=%s character=0x%08lx wrapper=0x%08lx renderer=0x%08lx previous=%s policy=release_presentation_ownership_without_animation_write\r\n",
            reason == NULL ? "unspecified" : reason,
            (unsigned long)(uintptr_t)
                spirit_player_two_melee_locomotion_character,
            (unsigned long)(uintptr_t)
                spirit_player_two_melee_locomotion_wrapper,
            (unsigned long)(uintptr_t)
                spirit_player_two_melee_locomotion_renderer,
            spirit_player_two_melee_locomotion_moving ? "moving" : "idle"
        );
    }
    spirit_player_two_melee_locomotion_owned = FALSE;
    spirit_player_two_melee_locomotion_moving = FALSE;
    spirit_player_two_melee_locomotion_character = NULL;
    spirit_player_two_melee_locomotion_wrapper = NULL;
    spirit_player_two_melee_locomotion_renderer = NULL;
}

static BOOL spirit_player_two_melee_locomotion_matches(
    void *renderer,
    const SudekiMpRangedWorldAnimationMethods *methods,
    unsigned int submodels,
    BOOL moving
) {
    const int selector_zero = moving ? 36 : 17;
    const int selector_one = moving ? 32 : 0;
    const float rate_zero = moving ? 37.1709f : 12.0f;
    const float rate_one = moving ? 30.9758f : 0.0f;
    const float blend_zero = moving ? 0.99f : 0.0f;
    unsigned int submodel;
    float blend;

    for (submodel = 0u; submodel < submodels; ++submodel) {
        float current_rate_zero = methods->get_rate(
            renderer, 0, submodel
        );
        float current_rate_one = methods->get_rate(
            renderer, 1, submodel
        );

        if (methods->get_selector(renderer, 0, submodel) != selector_zero ||
            methods->get_selector(renderer, 1, submodel) != selector_one ||
            !isfinite(current_rate_zero) || !isfinite(current_rate_one) ||
            fabsf(current_rate_zero - rate_zero) > 0.001f ||
            fabsf(current_rate_one - rate_one) > 0.001f) {
            return FALSE;
        }
    }
    blend = methods->get_blend(renderer, 0);
    return isfinite(blend) && fabsf(blend - blend_zero) <= 0.001f;
}

/*
 * Sudeki globally holds every party member in its current presentation pose
 * during a Spirit Strike.  The control bridge separately restores the
 * non-caster's collision-aware translation.  For Tal only, mirror the exact
 * native full-speed combat locomotion presentation observed outside Spirit:
 * selector 36 on channel 0 and selector 32 on channel 1.  This touches no
 * gameplay controller and never ticks the renderer a second time.
 */
static void compose_spirit_player_two_melee_locomotion(void) {
    uint8_t *character = (uint8_t *)player_two_character;
    uint8_t *position;
    uint8_t *wrapper;
    void *renderer;
    SudekiMpRangedWorldAnimationMethods methods;
    const char *reason = "unknown";
    unsigned int submodels;
    int channel_count;
    BOOL moving;
    BOOL ownership_changed;

    if (!SudekiMpSplitScreenPlayerTwoIsNonCasterDuringSpirit(character) ||
        !character_has_resource_type(character, 0x23u)) {
        reset_spirit_player_two_melee_locomotion("ownership_inactive");
        return;
    }
    position = readable_memory(character, 0x48u) ?
        *(uint8_t **)(character + 0x44u) : NULL;
    wrapper = readable_memory(position, 0xb8u) ?
        *(uint8_t **)(position + 0xb4u) : NULL;
    renderer = readable_memory(wrapper, 0x14u) ?
        *(void **)(wrapper + 0x10u) : NULL;
    if (!readable_memory(renderer, 0xa8u) ||
        *(void **)renderer != game_base + RVA_ANIMATION_RENDERER_VTABLE ||
        !ranged_world_animation_methods(renderer, &methods, &reason)) {
        reset_spirit_player_two_melee_locomotion(
            reason == NULL ? "renderer_gate_failed" : reason
        );
        return;
    }
    channel_count = *(int *)((uint8_t *)renderer + 0xa0u);
    submodels = methods.count(renderer);
    if (channel_count < 2 || channel_count > 32 ||
        submodels == 0u || submodels > 32u) {
        reset_spirit_player_two_melee_locomotion(
            "renderer_count_gate_failed"
        );
        return;
    }
    moving = SudekiMpControlSeparationSecondPlayerMovementActive() &&
        SudekiMpControlSeparationSecondPlayerMovementMagnitude() > 0.0001f;
    ownership_changed =
        !spirit_player_two_melee_locomotion_owned ||
        spirit_player_two_melee_locomotion_character != character ||
        spirit_player_two_melee_locomotion_wrapper != wrapper ||
        spirit_player_two_melee_locomotion_renderer != renderer;
    if (ownership_changed) {
        reset_spirit_player_two_melee_locomotion("ownership_changed");
        spirit_player_two_melee_locomotion_owned = TRUE;
        spirit_player_two_melee_locomotion_character = character;
        spirit_player_two_melee_locomotion_wrapper = wrapper;
        spirit_player_two_melee_locomotion_renderer = renderer;
    }
    if (!ownership_changed &&
        spirit_player_two_melee_locomotion_moving == moving &&
        spirit_player_two_melee_locomotion_matches(
            renderer, &methods, submodels, moving
        )) {
        return;
    }

    set_ranged_world_animation_channel(
        renderer,
        &methods,
        submodels,
        0,
        moving ? 36 : 17,
        0,
        moving ? 37.1709f : 12.0f
    );
    set_ranged_world_animation_channel(
        renderer,
        &methods,
        submodels,
        1,
        moving ? 32 : 0,
        moving ? 0 : 192,
        moving ? 30.9758f : 0.0f
    );
    methods.set_blend(renderer, 0, moving ? 0.99f : 0.0f);
    if (!spirit_player_two_melee_locomotion_matches(
            renderer, &methods, submodels, moving)) {
        reset_spirit_player_two_melee_locomotion(
            "setter_verification_failed"
        );
        return;
    }
    SudekiMpLogFormat(
        "split_screen_render event=spirit_player_two_melee_locomotion phase=applied reason=%s character=0x%08lx wrapper=0x%08lx renderer=0x%08lx submodels=%u state=%s selector_ch0=%d selector_ch1=%d rate_ch0_bits=0x%08lx rate_ch1_bits=0x%08lx blend0_bits=0x%08lx policy=tal_only_native_observed_presentation_no_gameplay_controller_or_manual_tick\r\n",
        ownership_changed ? "initial" :
            (spirit_player_two_melee_locomotion_moving != moving ?
                "movement_transition" : "renderer_drift"),
        (unsigned long)(uintptr_t)character,
        (unsigned long)(uintptr_t)wrapper,
        (unsigned long)(uintptr_t)renderer,
        submodels,
        moving ? "moving" : "idle",
        moving ? 36 : 17,
        moving ? 32 : 0,
        (unsigned long)float_bits(moving ? 37.1709f : 12.0f),
        (unsigned long)float_bits(moving ? 30.9758f : 0.0f),
        (unsigned long)float_bits(moving ? 0.99f : 0.0f)
    );
    spirit_player_two_melee_locomotion_moving = moving;
}

/*
 * The native ranged UI transition can clear the detached world renderer
 * without changing our movement classification.  Compare only the base
 * channels that this compositor owns.  State and time are deliberately not
 * part of this steady-state gate: Sudeki transiently sets the state 0x80 bit
 * when a looping clip wraps, and the native renderer owns clock advancement.
 * Channel 4 and blend 3 remain exclusively owned by the fire lifecycle.
 */
static BOOL ranged_world_base_matches(
    void *renderer,
    const SudekiMpRangedWorldAnimationMethods *methods,
    unsigned int submodels,
    BOOL moving,
    int idle_selector,
    int walk_selector,
    int run_selector
) {
    const int selectors[4] = {
        moving ? walk_selector : idle_selector,
        moving ? run_selector : 0,
        0,
        0
    };
    const float rates[4] = {
        moving ? 24.0f : 12.0f,
        moving ? 18.0f : 0.0f,
        0.0f,
        0.0f
    };
    const float blends[3] = {
        moving ? 0.99f : 0.0f,
        0.0f,
        0.0f
    };
    unsigned int channel;
    unsigned int submodel;

    for (channel = 0u; channel < 4u; ++channel) {
        for (submodel = 0u; submodel < submodels; ++submodel) {
            float rate = methods->get_rate(
                renderer, (int)channel, submodel
            );

            if (methods->get_selector(
                    renderer, (int)channel, submodel) !=
                    selectors[channel] ||
                !isfinite(rate) ||
                fabsf(rate - rates[channel]) > 0.0001f) {
                return FALSE;
            }
        }
    }
    for (channel = 0u; channel < 3u; ++channel) {
        float blend = methods->get_blend(renderer, (int)channel);

        if (!isfinite(blend) ||
            fabsf(blend - blends[channel]) > 0.0001f) {
            return FALSE;
        }
    }
    return TRUE;
}

static BOOL apply_ranged_world_base_transition(
    void *renderer,
    const SudekiMpRangedWorldAnimationMethods *methods,
    unsigned int submodels,
    BOOL moving,
    BOOL action_playing
) {
    const int dormant_state = 192;
    float blend_zero;
    float blend_three;

    if (moving) {
        set_ranged_world_animation_channel(
            renderer, methods, submodels, 0, 22, 0, 24.0f
        );
        set_ranged_world_animation_channel(
            renderer, methods, submodels, 1, 23, 0, 18.0f
        );
    } else {
        set_ranged_world_animation_channel(
            renderer, methods, submodels, 0, 20, 0, 12.0f
        );
        set_ranged_world_animation_channel(
            renderer, methods, submodels, 1, 0, dormant_state, 0.0f
        );
    }
    set_ranged_world_animation_channel(
        renderer, methods, submodels, 2, 0, dormant_state, 0.0f
    );
    set_ranged_world_animation_channel(
        renderer, methods, submodels, 3, 0, dormant_state, 0.0f
    );
    methods->set_blend(renderer, 0, moving ? 0.99f : 0.0f);
    methods->set_blend(renderer, 1, 0.0f);
    methods->set_blend(renderer, 2, 0.0f);
    methods->set_blend(renderer, 3, action_playing ? 1.0f : 0.0f);

    if (!verify_ranged_world_animation_channel(
            renderer, methods, submodels, 0,
            moving ? 22 : 20, 0u, moving ? 24.0f : 12.0f) ||
        !verify_ranged_world_animation_channel(
            renderer, methods, submodels, 1,
            moving ? 23 : 0, moving ? 0u : 192u,
            moving ? 18.0f : 0.0f) ||
        !verify_ranged_world_animation_channel(
            renderer, methods, submodels, 2, 0, 192u, 0.0f) ||
        !verify_ranged_world_animation_channel(
            renderer, methods, submodels, 3, 0, 192u, 0.0f)) {
        return FALSE;
    }
    blend_zero = methods->get_blend(renderer, 0);
    blend_three = methods->get_blend(renderer, 3);
    return isfinite(blend_zero) && isfinite(blend_three) &&
        fabsf(blend_zero - (moving ? 0.99f : 0.0f)) <= 0.0001f &&
        fabsf(methods->get_blend(renderer, 1)) <= 0.0001f &&
        fabsf(methods->get_blend(renderer, 2)) <= 0.0001f &&
        fabsf(blend_three - (action_playing ? 1.0f : 0.0f)) <= 0.0001f;
}

static BOOL start_ranged_world_action(
    void *renderer,
    const SudekiMpRangedWorldAnimationMethods *methods,
    unsigned int submodels,
    int selector
) {
    set_ranged_world_animation_channel(
        renderer, methods, submodels, 4, selector, 1, 24.0f
    );
    methods->set_blend(renderer, 3, 1.0f);
    return verify_ranged_world_animation_channel(
            renderer, methods, submodels, 4, selector, 1u, 24.0f) &&
        isfinite(methods->get_blend(renderer, 3)) &&
        fabsf(methods->get_blend(renderer, 3) - 1.0f) <= 0.0001f;
}

static BOOL cleanup_ranged_world_action(
    void *renderer,
    const SudekiMpRangedWorldAnimationMethods *methods,
    unsigned int submodels
) {
    set_ranged_world_animation_channel(
        renderer, methods, submodels, 4, 0, 192, 0.0f
    );
    methods->set_blend(renderer, 3, 0.0f);
    return verify_ranged_world_animation_channel(
            renderer, methods, submodels, 4, 0, 192u, 0.0f) &&
        isfinite(methods->get_blend(renderer, 3)) &&
        fabsf(methods->get_blend(renderer, 3)) <= 0.0001f;
}

static BOOL compose_ranged_world_animation(
    void *character,
    uint8_t *component,
    void *first_person_wrapper,
    void *world_wrapper
) {
    static const char rejection_renderer_changed[] =
        "renderer_changed_during_ownership";
    static const char rejection_count[] = "submodel_count_invalid";
    static const char rejection_count_changed[] =
        "submodel_count_changed_during_ownership";
    static const char rejection_base_verify[] =
        "base_transition_getter_verification_failed";
    static const char rejection_action_verify[] =
        "action_transition_getter_verification_failed";
    static const DWORD action_timeout_ms = 1500u;
    SudekiMpRangedWorldAnimationMethods methods;
    ModelAnimationLookupFunction lookup;
    const char *reason;
    void *renderer;
    unsigned int submodels;
    int idle_selector;
    int walk_selector;
    int run_selector;
    int standard_action_selector;
    int shock_action_selector;
    float speed;
    BOOL movement_gate;
    BOOL moving;
    BOOL standard_source_present;
    BOOL shock_source_present;
    BOOL standard_source_rising;
    BOOL shock_source_rising;
    BOOL action_replaced;
    BOOL active_source_present;
    unsigned int requested_source_id;
    unsigned int requested_target_id;
    int requested_selector;
    BOOL initial_base;
    BOOL base_drifted;
    const char *base_transition_reason;
    DWORD now;

    claim_ranged_world_compositor(
        character,
        component,
        first_person_wrapper,
        world_wrapper
    );
    reason = "unknown";
    if (!ranged_state_details_renderer(
            world_wrapper,
            &renderer,
            &lookup,
            &reason)) {
        return reject_ranged_world_compositor(reason);
    }
    if (ranged_world_compositor_state.world_renderer != NULL &&
        ranged_world_compositor_state.world_renderer != renderer) {
        reset_ranged_world_compositor(rejection_renderer_changed);
        claim_ranged_world_compositor(
            character,
            component,
            first_person_wrapper,
            world_wrapper
        );
        ranged_world_compositor_state.world_renderer = renderer;
        return reject_ranged_world_compositor(rejection_renderer_changed);
    }
    ranged_world_compositor_state.world_renderer = renderer;
    if (ranged_world_compositor_state.write_attempted &&
        !ranged_world_compositor_state.applied) {
        return FALSE;
    }
    if (!ranged_world_animation_methods(renderer, &methods, &reason)) {
        return reject_ranged_world_compositor(reason);
    }
    if (!resolve_ranged_world_animation_selector(
            component, renderer, lookup, 0x02u, 20,
            &idle_selector, &reason) ||
        !resolve_ranged_world_animation_selector(
            component, renderer, lookup, 0x06u, 22,
            &walk_selector, &reason) ||
        !resolve_ranged_world_animation_selector(
            component, renderer, lookup, 0x07u, 23,
            &run_selector, &reason) ||
        !resolve_ranged_world_animation_selector(
            component, renderer, lookup, 0x85u, 59,
            &standard_action_selector, &reason) ||
        !resolve_ranged_world_animation_selector(
            component, renderer, lookup, 0x87u, 55,
            &shock_action_selector, &reason)) {
        return reject_ranged_world_compositor(reason);
    }
    submodels = methods.count(renderer);
    if (submodels == 0u || submodels > 32u) {
        return reject_ranged_world_compositor(rejection_count);
    }
    if (ranged_world_compositor_state.applied &&
        ranged_world_compositor_state.submodels != submodels) {
        return reject_ranged_world_compositor(rejection_count_changed);
    }
    if (!classify_ranged_world_movement(
            character,
            &speed,
            &movement_gate,
            &moving,
            &reason) ||
        !ranged_source_animation_present(
            component, 0x8cu, &standard_source_present, &reason) ||
        !ranged_source_animation_present(
            component, 0x8eu, &shock_source_present, &reason)) {
        return reject_ranged_world_compositor(reason);
    }

    initial_base = !ranged_world_compositor_state.applied;
    base_drifted = !initial_base &&
        ranged_world_compositor_state.moving == moving &&
        !ranged_world_base_matches(
            renderer,
            &methods,
            submodels,
            moving,
            idle_selector,
            walk_selector,
            run_selector
        );
    if (initial_base || ranged_world_compositor_state.moving != moving ||
        base_drifted) {
        base_transition_reason = initial_base ? "initialization" :
            (ranged_world_compositor_state.moving != moving ?
                "movement_state_changed" : "renderer_drift");
        ranged_world_compositor_state.write_attempted = TRUE;
        if (!apply_ranged_world_base_transition(
                renderer,
                &methods,
                submodels,
                moving,
                ranged_world_compositor_state.action_playing)) {
            return reject_ranged_world_compositor(rejection_base_verify);
        }
        ranged_world_compositor_state.applied = TRUE;
        ranged_world_compositor_state.moving = moving;
        ranged_world_compositor_state.submodels = submodels;
        ranged_world_compositor_state.idle_selector = idle_selector;
        ranged_world_compositor_state.walk_selector = walk_selector;
        ranged_world_compositor_state.run_selector = run_selector;
        ranged_world_compositor_state.standard_action_selector =
            standard_action_selector;
        ranged_world_compositor_state.shock_action_selector =
            shock_action_selector;
        ranged_world_compositor_state.last_rejection = NULL;
        SudekiMpLogFormat(
            "split_screen_render event=ranged_world_compositor phase=base_transition reason=%s character=0x%08lx world_renderer=0x%08lx mode=%s initial=%s movement_gate=%s speed=%.4f submodels=%u channels=ch0:%d@%.1f,ch1:%d@%.1f,ch2:dormant,ch3:dormant blends=%.2f,0,0,%s selectors_exact=id02:%d,id06:%d,id07:%d,id85:%d,id87:%d time_policy=reset_only_on_transition clock_policy=native_renderer_tick_only crossfade=disabled_hard_cut drift_scope=channels_0_3_selectors_rates_and_blends_0_2_excludes_state_time_channel_4_blend_3 controller_policy=no_gameplay_controller_call root_policy=no_pitch_or_root_write\r\n",
            base_transition_reason,
            (unsigned long)(uintptr_t)character,
            (unsigned long)(uintptr_t)renderer,
            moving ? "moving" : "idle",
            initial_base ? "true" : "false",
            movement_gate ? "true" : "false",
            speed,
            submodels,
            moving ? walk_selector : idle_selector,
            moving ? 24.0 : 12.0,
            moving ? run_selector : 0,
            moving ? 18.0 : 0.0,
            moving ? 0.99 : 0.0,
            ranged_world_compositor_state.action_playing ? "1" : "0",
            idle_selector,
            walk_selector,
            run_selector,
            standard_action_selector,
            shock_action_selector
        );
    }

    standard_source_rising = standard_source_present &&
        !ranged_world_compositor_state.standard_source_present;
    shock_source_rising = shock_source_present &&
        !ranged_world_compositor_state.shock_source_present;
    requested_source_id = 0u;
    requested_target_id = 0u;
    requested_selector = 0;
    if (shock_source_rising) {
        requested_source_id = 0x8eu;
        requested_target_id = 0x87u;
        requested_selector = shock_action_selector;
    } else if (standard_source_rising) {
        requested_source_id = 0x8cu;
        requested_target_id = 0x85u;
        requested_selector = standard_action_selector;
    }
    now = GetTickCount();
    if (requested_source_id != 0u) {
        action_replaced = ranged_world_compositor_state.action_playing;
        ranged_world_compositor_state.write_attempted = TRUE;
        if (!start_ranged_world_action(
                renderer, &methods, submodels, requested_selector)) {
            return reject_ranged_world_compositor(
                rejection_action_verify
            );
        }
        ranged_world_compositor_state.action_playing = TRUE;
        ranged_world_compositor_state.action_source_id =
            requested_source_id;
        ranged_world_compositor_state.action_target_id =
            requested_target_id;
        ranged_world_compositor_state.action_selector =
            requested_selector;
        ranged_world_compositor_state.action_started_tick = now;
        ranged_world_compositor_state.last_rejection = NULL;
        SudekiMpLogFormat(
            "split_screen_render event=ranged_world_compositor phase=action_start character=0x%08lx world_renderer=0x%08lx semantic=%s source_id=0x%02x target_id=0x%02x channel=4 selector=%d state=1 rate=24.0000 blend_channel=3 blend=1.0000 rising_edge=true replaced_or_restarted=%s migration_policy=presence_across_all_channels_no_restart_on_channel_2_to_0 clock_policy=native_renderer_tick_only\r\n",
            (unsigned long)(uintptr_t)character,
            (unsigned long)(uintptr_t)renderer,
            requested_source_id == 0x8eu ? "shock_fire" :
                "standard_fire",
            requested_source_id,
            requested_target_id,
            requested_selector,
            action_replaced ? "true" : "false"
        );
    } else if (ranged_world_compositor_state.action_playing) {
        unsigned int action_state = methods.get_state(
            renderer, 4, 0u
        );
        DWORD elapsed = now -
            ranged_world_compositor_state.action_started_tick;
        BOOL renderer_complete = (action_state & 0x40u) != 0u;
        BOOL timed_out = elapsed >= action_timeout_ms;

        active_source_present =
            (ranged_world_compositor_state.action_source_id == 0x8cu &&
             standard_source_present) ||
            (ranged_world_compositor_state.action_source_id == 0x8eu &&
             shock_source_present);

        if (!active_source_present && (renderer_complete || timed_out)) {
            unsigned int completed_source_id =
                ranged_world_compositor_state.action_source_id;
            unsigned int completed_target_id =
                ranged_world_compositor_state.action_target_id;
            int completed_selector =
                ranged_world_compositor_state.action_selector;

            ranged_world_compositor_state.write_attempted = TRUE;
            if (!cleanup_ranged_world_action(
                    renderer, &methods, submodels)) {
                return reject_ranged_world_compositor(
                    rejection_action_verify
                );
            }
            ranged_world_compositor_state.action_playing = FALSE;
            ranged_world_compositor_state.action_source_id = 0u;
            ranged_world_compositor_state.action_target_id = 0u;
            ranged_world_compositor_state.action_selector = 0;
            ranged_world_compositor_state.last_rejection = NULL;
            SudekiMpLogFormat(
                "split_screen_render event=ranged_world_compositor phase=action_cleanup character=0x%08lx world_renderer=0x%08lx semantic=%s source_id=0x%02x target_id=0x%02x completed_selector=%d reason=%s observed_state=%u elapsed_ms=%lu channel=4 selector=0 state=192 rate=0 blend_channel=3 blend=0 timeout_ms=%lu\r\n",
                (unsigned long)(uintptr_t)character,
                (unsigned long)(uintptr_t)renderer,
                completed_source_id == 0x8eu ? "shock_fire" :
                    "standard_fire",
                completed_source_id,
                completed_target_id,
                completed_selector,
                renderer_complete ? "renderer_complete" : "timeout",
                action_state,
                (unsigned long)elapsed,
                (unsigned long)action_timeout_ms
            );
        }
    }
    ranged_world_compositor_state.standard_source_present =
        standard_source_present;
    ranged_world_compositor_state.shock_source_present =
        shock_source_present;
    return TRUE;
}

static BOOL ranged_world_compositor_eligible(
    void *character,
    uint8_t *position,
    uint8_t *component,
    void *first_person_wrapper,
    void *world_wrapper,
    BOOL show_first_person
) {
    return !fixed_three_assignment_selected() &&
        !show_first_person && rendered_player_two_this_frame &&
        character == player_one_character &&
        character_has_resource_type(character, 0x01u) &&
        readable_memory(position, 0xb8u) &&
        readable_memory(component, 0x168u) &&
        first_person_wrapper != world_wrapper &&
        *(void **)(component + 0x160u) == first_person_wrapper &&
        *(void **)(component + 0x164u) == world_wrapper &&
        *(void **)(position + 0xb4u) == first_person_wrapper &&
        *(void **)(position + 0xb4u) != world_wrapper;
}

/*
 * IDs 0x97/0x98/0x99 are registered MISSILE_AIM names/property IDs. Read any
 * matching loaded StateDetails pointers directly from the component table and
 * ask both retained renderers whether the two animation handles exist.
 * This deliberately avoids the StateDetails lookup/state-machine path and
 * never selects, plays, advances, or writes an animation.
 */
static void log_ranged_state_details_inventory(
    void *character,
    uint8_t *component,
    void *first_person_wrapper,
    void *world_wrapper
) {
    static const unsigned int animation_ids[] = {0x97u, 0x98u, 0x99u};
    uint8_t *animation_table;
    void *first_person_renderer;
    void *world_renderer;
    ModelAnimationLookupFunction first_person_lookup;
    ModelAnimationLookupFunction world_lookup;
    const char *first_person_reason;
    const char *world_reason;
    BOOL first_person_valid;
    BOOL world_valid;
    unsigned int index;

    if (ranged_state_details_inventory_complete ||
        !character_has_resource_type(character, 0x01u) ||
        !readable_memory(component, 0xe0u)) {
        return;
    }
    animation_table = *(uint8_t **)(component + 0xdcu);
    if (!readable_memory(
            animation_table,
            0x14u + (animation_ids[2] + 1u) * sizeof(void *))) {
        return;
    }

    first_person_reason = "unknown";
    world_reason = "unknown";
    first_person_valid = ranged_state_details_renderer(
        first_person_wrapper,
        &first_person_renderer,
        &first_person_lookup,
        &first_person_reason
    );
    world_valid = ranged_state_details_renderer(
        world_wrapper,
        &world_renderer,
        &world_lookup,
        &world_reason
    );
    if (!first_person_valid || !world_valid) {
        if (!ranged_state_details_inventory_pending_logged) {
            ranged_state_details_inventory_pending_logged = TRUE;
            SudekiMpLogFormat(
                "split_screen_render event=ranged_state_details_inventory phase=pending first_person_renderer=0x%08lx world_renderer=0x%08lx first_person_reason=%s world_reason=%s policy=retry_exact_renderer_gate_without_state_lookup_or_write\r\n",
                (unsigned long)(uintptr_t)first_person_renderer,
                (unsigned long)(uintptr_t)world_renderer,
                first_person_reason,
                world_reason
            );
        }
        return;
    }
    ranged_state_details_inventory_pending_logged = FALSE;

    for (index = 0u;
         index < sizeof(animation_ids) / sizeof(animation_ids[0]);
         ++index) {
        unsigned int animation_id = animation_ids[index];
        uint8_t *details = *(uint8_t **)(
            animation_table + 0x14u + animation_id * sizeof(void *)
        );
        uint32_t handle_14;
        uint32_t ref_18;
        uint32_t handle_20;
        uint32_t ref_24;
        unsigned int byte_59;

        if (!readable_memory(details, 0x5au)) {
            SudekiMpLogFormat(
                "split_screen_render event=ranged_state_details_inventory animation_id=0x%02x result=rejected details=0x%08lx first_person_renderer=0x%08lx world_renderer=0x%08lx first_person_reason=%s world_reason=%s details_reason=%s policy=one_time_direct_table_read_no_state_lookup_or_animation_write\r\n",
                animation_id,
                (unsigned long)(uintptr_t)details,
                (unsigned long)(uintptr_t)first_person_renderer,
                (unsigned long)(uintptr_t)world_renderer,
                first_person_reason,
                world_reason,
                readable_memory(details, 0x5au) ? "readable" : "unreadable"
            );
            continue;
        }

        memcpy(&handle_14, details + 0x14u, sizeof(handle_14));
        memcpy(&ref_18, details + 0x18u, sizeof(ref_18));
        memcpy(&handle_20, details + 0x20u, sizeof(handle_20));
        memcpy(&ref_24, details + 0x24u, sizeof(ref_24));
        byte_59 = details[0x59u];
        SudekiMpLogFormat(
            "split_screen_render event=ranged_state_details_inventory animation_id=0x%02x result=resolved details=0x%08lx handle_14=0x%08lx fp_14=%d world_14=%d ref_18=0x%08lx handle_20=0x%08lx fp_20=%d world_20=%d ref_24=0x%08lx field_59=0x%02x policy=one_time_direct_table_read_exact_renderer_handle_lookup_no_state_lookup_selector_playback_or_animation_write\r\n",
            animation_id,
            (unsigned long)(uintptr_t)details,
            (unsigned long)handle_14,
            resolve_ranged_state_details_handle(
                first_person_renderer, first_person_lookup, handle_14),
            resolve_ranged_state_details_handle(
                world_renderer, world_lookup, handle_14),
            (unsigned long)ref_18,
            (unsigned long)handle_20,
            resolve_ranged_state_details_handle(
                first_person_renderer, first_person_lookup, handle_20),
            resolve_ranged_state_details_handle(
                world_renderer, world_lookup, handle_20),
            (unsigned long)ref_24,
            byte_59
        );
    }
    ranged_state_details_inventory_complete = TRUE;
}

static void reset_ranged_animation_trace(void) {
    ZeroMemory(ranged_animation_trace, sizeof(ranged_animation_trace));
    ZeroMemory(
        ranged_animation_trace_sequence,
        sizeof(ranged_animation_trace_sequence)
    );
    ZeroMemory(
        ranged_animation_progress_frame,
        sizeof(ranged_animation_progress_frame)
    );
    ZeroMemory(
        ranged_animation_capture_failure_character,
        sizeof(ranged_animation_capture_failure_character)
    );
}

static void reset_hud_mapping_trace(void) {
    ZeroMemory(hud_mapping_trace_valid, sizeof(hud_mapping_trace_valid));
    ZeroMemory(
        hud_mapping_trace_resolved_slot,
        sizeof(hud_mapping_trace_resolved_slot)
    );
}

static void reset_minimap_owner_schedule(void) {
    minimap_scheduled_owner_valid = FALSE;
    minimap_scheduled_player_two = FALSE;
    minimap_update_owner_valid = FALSE;
    minimap_update_player_two = FALSE;
    minimap_update_character = NULL;
    minimap_source_failure_this_frame = FALSE;
    minimap_owner_mismatch_logged = FALSE;
    ZeroMemory(minimap_ownership_logged, sizeof(minimap_ownership_logged));
}

static void latch_minimap_scheduled_owner(void) {
    /* The update owner belongs only to the frame that just finished.  The
     * next frame must prove a fresh native UIMapManager::Update before its
     * map render is allowed to replace either cached viewport. */
    minimap_update_owner_valid = FALSE;
    minimap_update_player_two = FALSE;
    minimap_update_character = NULL;
    minimap_scheduled_owner_valid =
        dual_camera_frame_cache_enabled &&
        second_player_camera_enabled &&
        runtime_split_enabled &&
        player_two_camera != NULL &&
        player_one_character != NULL &&
        player_two_character != NULL &&
        player_one_character != player_two_character &&
        player_two_party_slot > 0u &&
        player_two_party_slot < PARTY_SLOT_COUNT &&
        readable_memory(player_one_character, 0x94u) &&
        readable_memory(player_two_character, 0x94u);
    minimap_scheduled_player_two =
        minimap_scheduled_owner_valid && player_two_view_requested;
    minimap_source_failure_this_frame = FALSE;
}

static BOOL read_animation_interface_state(
    void *wrapper,
    int selectors[RANGED_ANIMATION_CHANNEL_COUNT],
    unsigned int states[RANGED_ANIMATION_CHANNEL_COUNT]
) {
    void *model_interface;
    ModelAnimationCountFunction count;
    ModelAnimationSelectorGetFunction get_selector;
    ModelAnimationStateGetFunction get_state;
    unsigned int submodels;
    unsigned int channel;

    if (!readable_memory(wrapper, 0x14u)) {
        return FALSE;
    }
    model_interface = *(void **)((uint8_t *)wrapper + 0x10u);
    count = (ModelAnimationCountFunction)
        model_animation_method(model_interface, 0xf8u);
    get_selector = (ModelAnimationSelectorGetFunction)
        model_animation_method(model_interface, 0x100u);
    get_state = (ModelAnimationStateGetFunction)
        model_animation_method(model_interface, 0x118u);
    if (count == NULL || get_selector == NULL || get_state == NULL) {
        return FALSE;
    }
    submodels = count(model_interface);
    if (submodels == 0u || submodels > 32u) {
        return FALSE;
    }
    for (channel = 0u; channel < RANGED_ANIMATION_CHANNEL_COUNT; ++channel) {
        selectors[channel] = get_selector(
            model_interface,
            (int)channel,
            0u
        );
        states[channel] = get_state(
            model_interface,
            (int)channel,
            0u
        );
    }
    return TRUE;
}

static BOOL capture_ranged_animation_trace(
    void *character_pointer,
    SudekiMpAnimationTraceSnapshot *snapshot
) {
    uint8_t *character = (uint8_t *)character_pointer;
    uint8_t *arbiter;
    uint8_t *position;
    uint8_t *component;
    uint8_t *animation_state;
    unsigned int channel;

    ZeroMemory(snapshot, sizeof(*snapshot));
    if (!readable_memory(character, 0x138u)) {
        return FALSE;
    }
    arbiter = *(uint8_t **)(character + 0x90u);
    position = *(uint8_t **)(character + 0x44u);
    component = *(uint8_t **)(character + 0x134u);
    if (!readable_memory(arbiter, 0x54u) ||
        !readable_memory(position, 0xb8u) ||
        !readable_memory(component, 0x168u)) {
        return FALSE;
    }
    snapshot->character = character;
    snapshot->component = component;
    snapshot->weapon = readable_memory(character, 0xc4u) ?
        *(void **)(character + 0xc0u) : NULL;
    snapshot->weapon_item = readable_memory(snapshot->weapon, 0x26cu) ?
        *(void **)((uint8_t *)snapshot->weapon + 0x268u) : NULL;
    snapshot->attached_wrapper = *(void **)(position + 0xb4u);
    snapshot->first_person_wrapper = *(void **)(component + 0x160u);
    snapshot->world_wrapper = *(void **)(component + 0x164u);
    snapshot->first_person_render_object =
        readable_memory(snapshot->first_person_wrapper, 0x0cu) ?
            *(void **)((uint8_t *)snapshot->first_person_wrapper + 0x08u) :
            NULL;
    snapshot->world_render_object =
        readable_memory(snapshot->world_wrapper, 0x0cu) ?
            *(void **)((uint8_t *)snapshot->world_wrapper + 0x08u) : NULL;
    snapshot->first_person_render_flags =
        readable_memory(snapshot->first_person_render_object, 0x38u) ?
            *(uint32_t *)((uint8_t *)snapshot->first_person_render_object +
                0x34u) : 0u;
    snapshot->world_render_flags =
        readable_memory(snapshot->world_render_object, 0x38u) ?
            *(uint32_t *)((uint8_t *)snapshot->world_render_object + 0x34u) :
            0u;
    snapshot->first_person_active =
        (*(uint32_t *)(arbiter + 0x50u) &
         RANGED_FIRST_PERSON_ARBITER_FLAG) != 0u;
    snapshot->animation_bank = (component[0x133u] & 2u) != 0u;
    if (snapshot->first_person_wrapper == snapshot->world_wrapper ||
        !readable_memory(snapshot->first_person_wrapper, 0x14u) ||
        !readable_memory(snapshot->world_wrapper, 0x14u)) {
        return FALSE;
    }
    animation_state = *(uint8_t **)(component + 0xf8u);
    if (!readable_memory(
            animation_state,
            RANGED_ANIMATION_CHANNEL_COUNT * 4u)) {
        return FALSE;
    }
    for (channel = 0u; channel < RANGED_ANIMATION_CHANNEL_COUNT; ++channel) {
        snapshot->animation_ids[channel] =
            animation_state[channel * 4u + 2u];
    }
    if (!read_animation_interface_state(
            snapshot->first_person_wrapper,
            snapshot->first_person_selectors,
            snapshot->first_person_states) ||
        !read_animation_interface_state(
            snapshot->world_wrapper,
            snapshot->world_selectors,
            snapshot->world_states)) {
        return FALSE;
    }
    snapshot->valid = TRUE;
    return TRUE;
}

static void log_ranged_animation_capture_failure(
    unsigned int player_index,
    void *character
) {
    uint8_t *character_bytes = (uint8_t *)character;
    uint8_t *position = NULL;
    uint8_t *component = NULL;
    void *first_person_wrapper = NULL;
    void *world_wrapper = NULL;
    const char *reason = "unknown";

    if (player_index >= 2u ||
        ranged_animation_capture_failure_character[player_index] ==
            character) {
        return;
    }
    ranged_animation_capture_failure_character[player_index] = character;
    if (!readable_memory(character, 0x138u)) {
        reason = "character_unreadable";
    } else {
        position = *(uint8_t **)(character_bytes + 0x44u);
        component = *(uint8_t **)(character_bytes + 0x134u);
        if (!readable_memory(position, 0xb8u) ||
            !readable_memory(component, 0x168u)) {
            reason = "position_or_component_unreadable";
        } else {
            first_person_wrapper = *(void **)(component + 0x160u);
            world_wrapper = *(void **)(component + 0x164u);
            if (first_person_wrapper == world_wrapper) {
                reason = "first_person_and_world_wrapper_equal";
            } else if (!readable_memory(first_person_wrapper, 0x14u) ||
                       !readable_memory(world_wrapper, 0x14u)) {
                reason = "wrapper_unreadable";
            } else if (!readable_memory(*(void **)(component + 0xf8u),
                                       RANGED_ANIMATION_CHANNEL_COUNT * 4u)) {
                reason = "animation_state_unreadable";
            } else {
                reason = "animation_interface_state_unreadable";
            }
        }
    }
    SudekiMpLogFormat(
        "split_screen_render event=ranged_animation_capture_failure player=%u character=0x%08lx component=0x%08lx position=0x%08lx first_person_wrapper=0x%08lx world_wrapper=0x%08lx reason=%s policy=read_only_diagnostic\r\n",
        player_index + 1u,
        (unsigned long)(uintptr_t)character,
        (unsigned long)(uintptr_t)component,
        (unsigned long)(uintptr_t)position,
        (unsigned long)(uintptr_t)first_person_wrapper,
        (unsigned long)(uintptr_t)world_wrapper,
        reason
    );
}

static void trace_ranged_animation_transition(
    unsigned int player_index,
    void *character
) {
    SudekiMpAnimationTraceSnapshot current;
    SudekiMpAnimationTraceSnapshot *previous;

    if (!split_screen_ranged_model_isolation_enabled ||
        player_index >= 2u) {
        return;
    }
    if (!capture_ranged_animation_trace(character, &current)) {
        log_ranged_animation_capture_failure(player_index, character);
        return;
    }
    previous = &ranged_animation_trace[player_index];
    if (previous->valid &&
        memcmp(previous, &current, sizeof(current)) == 0) {
        return;
    }
    *previous = current;
    ++ranged_animation_trace_sequence[player_index];
    SudekiMpLogFormat(
        "split_screen_render event=ranged_animation_trace player=%u sequence=%u character=0x%08lx component=0x%08lx weapon=0x%08lx weapon_item=0x%08lx attached=%s first_person_wrapper=0x%08lx world_wrapper=0x%08lx first_person_render=0x%08lx world_render=0x%08lx first_person_flags=0x%08lx world_flags=0x%08lx first_person_active=%u animation_bank=%s animation_ids=%02x,%02x,%02x,%02x,%02x first_person_selectors=%d,%d,%d,%d,%d world_selectors=%d,%d,%d,%d,%d first_person_states=%u,%u,%u,%u,%u world_states=%u,%u,%u,%u,%u policy=observation_before_player_two_render_borrow\r\n",
        player_index + 1u,
        ranged_animation_trace_sequence[player_index],
        (unsigned long)(uintptr_t)current.character,
        (unsigned long)(uintptr_t)current.component,
        (unsigned long)(uintptr_t)current.weapon,
        (unsigned long)(uintptr_t)current.weapon_item,
        current.attached_wrapper == current.first_person_wrapper ?
            "first_person" :
            (current.attached_wrapper == current.world_wrapper ?
                "world" : "other"),
        (unsigned long)(uintptr_t)current.first_person_wrapper,
        (unsigned long)(uintptr_t)current.world_wrapper,
        (unsigned long)(uintptr_t)current.first_person_render_object,
        (unsigned long)(uintptr_t)current.world_render_object,
        (unsigned long)current.first_person_render_flags,
        (unsigned long)current.world_render_flags,
        current.first_person_active,
        current.animation_bank ? "first" : "second",
        current.animation_ids[0],
        current.animation_ids[1],
        current.animation_ids[2],
        current.animation_ids[3],
        current.animation_ids[4],
        current.first_person_selectors[0],
        current.first_person_selectors[1],
        current.first_person_selectors[2],
        current.first_person_selectors[3],
        current.first_person_selectors[4],
        current.world_selectors[0],
        current.world_selectors[1],
        current.world_selectors[2],
        current.world_selectors[3],
        current.world_selectors[4],
        current.first_person_states[0],
        current.first_person_states[1],
        current.first_person_states[2],
        current.first_person_states[3],
        current.first_person_states[4],
        current.world_states[0],
        current.world_states[1],
        current.world_states[2],
        current.world_states[3],
        current.world_states[4]
    );
}

/*
 * The transition trace above intentionally logs only observable state changes.
 * A ranged character can still remain visually idle when the native selector
 * changed, so sample the animation clock separately at a low cadence.  This is
 * read-only diagnostics: no animation value is written here and no render
 * attachment is changed.
 */
static void trace_ranged_animation_progress(
    unsigned int player_index,
    void *character
) {
    SudekiMpAnimationTraceSnapshot snapshot;
    ModelAnimationValueGetFunction get_rate;
    ModelAnimationValueGetFunction get_time;
    ModelAnimationBlendGetFunction get_blend;
    void *first_person_interface;
    void *world_interface;
    unsigned int channel;
    float first_rates[RANGED_ANIMATION_CHANNEL_COUNT];
    float first_times[RANGED_ANIMATION_CHANNEL_COUNT];
    float first_blends[RANGED_ANIMATION_CHANNEL_COUNT];
    float world_rates[RANGED_ANIMATION_CHANNEL_COUNT];
    float world_times[RANGED_ANIMATION_CHANNEL_COUNT];
    float world_blends[RANGED_ANIMATION_CHANNEL_COUNT];

    if (!split_screen_ranged_model_isolation_enabled ||
        player_index >= 2u) {
        return;
    }
    if (!capture_ranged_animation_trace(character, &snapshot)) {
        log_ranged_animation_capture_failure(player_index, character);
        return;
    }
    ++ranged_animation_progress_frame[player_index];
    if ((ranged_animation_progress_frame[player_index] % 30u) != 0u) {
        return;
    }
    first_person_interface = *(void **)(
        (uint8_t *)snapshot.first_person_wrapper + 0x10u
    );
    world_interface = *(void **)(
        (uint8_t *)snapshot.world_wrapper + 0x10u
    );
    get_rate = (ModelAnimationValueGetFunction)
        model_animation_method(first_person_interface, 0x108u);
    get_time = (ModelAnimationValueGetFunction)
        model_animation_method(first_person_interface, 0x110u);
    get_blend = (ModelAnimationBlendGetFunction)
        model_animation_method(first_person_interface, 0x148u);
    if (get_rate == NULL || get_time == NULL || get_blend == NULL) {
        return;
    }
    for (channel = 0u; channel < RANGED_ANIMATION_CHANNEL_COUNT; ++channel) {
        first_rates[channel] = get_rate(
            first_person_interface, (int)channel, 0u
        );
        first_times[channel] = get_time(
            first_person_interface, (int)channel, 0u
        );
        first_blends[channel] = get_blend(
            first_person_interface, (int)channel
        );
    }
    get_rate = (ModelAnimationValueGetFunction)
        model_animation_method(world_interface, 0x108u);
    get_time = (ModelAnimationValueGetFunction)
        model_animation_method(world_interface, 0x110u);
    get_blend = (ModelAnimationBlendGetFunction)
        model_animation_method(world_interface, 0x148u);
    if (get_rate == NULL || get_time == NULL || get_blend == NULL) {
        return;
    }
    for (channel = 0u; channel < RANGED_ANIMATION_CHANNEL_COUNT; ++channel) {
        world_rates[channel] = get_rate(
            world_interface, (int)channel, 0u
        );
        world_times[channel] = get_time(
            world_interface, (int)channel, 0u
        );
        world_blends[channel] = get_blend(
            world_interface, (int)channel
        );
    }
    for (channel = 0u; channel < RANGED_ANIMATION_CHANNEL_COUNT; ++channel) {
        if (!isfinite(first_rates[channel]) ||
            !isfinite(first_times[channel]) ||
            !isfinite(first_blends[channel]) ||
            !isfinite(world_rates[channel]) ||
            !isfinite(world_times[channel]) ||
            !isfinite(world_blends[channel])) {
            return;
        }
    }
    SudekiMpLogFormat(
        "split_screen_render event=ranged_animation_progress player=%u character=0x%08lx attached=%s first_person_active=%u animation_bank=%s first_rate=%.4f,%.4f,%.4f,%.4f,%.4f first_time=%.4f,%.4f,%.4f,%.4f,%.4f first_blend=%.4f,%.4f,%.4f,%.4f,%.4f world_rate=%.4f,%.4f,%.4f,%.4f,%.4f world_time=%.4f,%.4f,%.4f,%.4f,%.4f world_blend=%.4f,%.4f,%.4f,%.4f,%.4f policy=read_only_clock_sample\r\n",
        player_index + 1u,
        (unsigned long)(uintptr_t)snapshot.character,
        snapshot.attached_wrapper == snapshot.first_person_wrapper ?
            "first_person" :
            (snapshot.attached_wrapper == snapshot.world_wrapper ?
                "world" : "other"),
        snapshot.first_person_active,
        snapshot.animation_bank ? "first" : "second",
        first_rates[0], first_rates[1], first_rates[2],
        first_rates[3], first_rates[4],
        first_times[0], first_times[1], first_times[2],
        first_times[3], first_times[4],
        first_blends[0], first_blends[1], first_blends[2],
        first_blends[3], first_blends[4],
        world_rates[0], world_rates[1], world_rates[2],
        world_rates[3], world_rates[4],
        world_times[0], world_times[1], world_times[2],
        world_times[3], world_times[4],
        world_blends[0], world_blends[1], world_blends[2],
        world_blends[3], world_blends[4]
    );
}

static BOOL sync_ranged_animation_model(
    void *character_pointer,
    uint8_t *component,
    uint8_t *source_wrapper,
    uint8_t *target_wrapper,
    BOOL target_is_first_person
) {
    void *first_person_interface;
    void *world_interface;
    ModelAnimationCountFunction first_person_count;
    ModelAnimationCountFunction world_count;
    ModelAnimationLookupFunction world_lookup;
    ModelAnimationSelectorGetFunction get_selector;
    ModelAnimationSelectorSetFunction set_selector;
    ModelAnimationValueGetFunction get_rate;
    ModelAnimationValueSetFunction set_rate;
    ModelAnimationValueGetFunction get_time;
    ModelAnimationTimeSetFunction set_time;
    ModelAnimationStateGetFunction get_state;
    ModelAnimationStateSetFunction set_state;
    ModelAnimationBlendGetFunction get_blend;
    ModelAnimationBlendSetFunction set_blend;
    uint8_t *world_render_object;
    uint32_t *world_flags;
    uint32_t saved_world_flags;
    uint32_t visible_world_flags;
    unsigned int first_person_submodels;
    unsigned int world_submodels;
    unsigned int channel;
    unsigned int submodel;
    unsigned int animation_ids[RANGED_ANIMATION_CHANNEL_COUNT];
    int selectors[RANGED_ANIMATION_CHANNEL_COUNT];
    uint32_t first_handles[RANGED_ANIMATION_CHANNEL_COUNT];
    uint32_t second_handles[RANGED_ANIMATION_CHANNEL_COUNT];
    int target_lookups[RANGED_ANIMATION_CHANNEL_COUNT];
    int source_lookups[RANGED_ANIMATION_CHANNEL_COUNT];
    BOOL ailish_character;

    first_person_interface = *(void **)(source_wrapper + 0x10u);
    world_interface = *(void **)(target_wrapper + 0x10u);
    world_render_object = *(uint8_t **)(target_wrapper + 0x08u);
    if (!writable_memory(world_render_object, 0x38u)) {
        return FALSE;
    }
    first_person_count = (ModelAnimationCountFunction)
        model_animation_method(first_person_interface, 0xf8u);
    world_count = (ModelAnimationCountFunction)
        model_animation_method(world_interface, 0xf8u);
    world_lookup = (ModelAnimationLookupFunction)
        model_animation_method(world_interface, 0x40u);
    get_selector = (ModelAnimationSelectorGetFunction)
        model_animation_method(first_person_interface, 0x100u);
    set_selector = (ModelAnimationSelectorSetFunction)
        model_animation_method(world_interface, 0xfcu);
    get_rate = (ModelAnimationValueGetFunction)
        model_animation_method(first_person_interface, 0x108u);
    set_rate = (ModelAnimationValueSetFunction)
        model_animation_method(world_interface, 0x104u);
    get_time = (ModelAnimationValueGetFunction)
        model_animation_method(first_person_interface, 0x110u);
    set_time = (ModelAnimationTimeSetFunction)
        model_animation_method(world_interface, 0x10cu);
    get_state = (ModelAnimationStateGetFunction)
        model_animation_method(first_person_interface, 0x118u);
    set_state = (ModelAnimationStateSetFunction)
        model_animation_method(world_interface, 0x114u);
    get_blend = (ModelAnimationBlendGetFunction)
        model_animation_method(first_person_interface, 0x148u);
    set_blend = (ModelAnimationBlendSetFunction)
        model_animation_method(world_interface, 0x144u);
    if (first_person_count == NULL || world_count == NULL ||
        world_lookup == NULL ||
        get_selector == NULL || set_selector == NULL ||
        get_rate == NULL || set_rate == NULL ||
        get_time == NULL || set_time == NULL ||
        get_state == NULL || set_state == NULL ||
        get_blend == NULL || set_blend == NULL) {
        return FALSE;
    }
    first_person_submodels = first_person_count(first_person_interface);
    world_submodels = world_count(world_interface);
    if (first_person_submodels == 0u || first_person_submodels > 32u ||
        world_submodels == 0u || world_submodels > 32u) {
        return FALSE;
    }

    ailish_character = character_has_resource_type(character_pointer, 0x01u);

    world_flags = (uint32_t *)(world_render_object + 0x34u);
    saved_world_flags = *world_flags;
    visible_world_flags = saved_world_flags & ~RENDER_OBJECT_HIDDEN_FLAG;
    *world_flags = visible_world_flags;
    if (ranged_translation_last_component != component) {
        ranged_translation_last_component = component;
        ZeroMemory(
            ranged_translation_trace_valid,
            sizeof(ranged_translation_trace_valid)
        );
    }
    for (channel = 0u; channel < RANGED_ANIMATION_CHANNEL_COUNT; ++channel) {
        int first_person_selector = get_selector(
            first_person_interface,
            (int)channel,
            0u
        );
        int selector = first_person_selector;
        float rate = get_rate(first_person_interface, (int)channel, 0u);
        float time = get_time(first_person_interface, (int)channel, 0u);
        unsigned int state = get_state(
            first_person_interface,
            (int)channel,
            0u
        );
        float blend = get_blend(first_person_interface, (int)channel);
        uint8_t *animation_state = readable_memory(component, 0xfcu) ?
            *(uint8_t **)(component + 0xf8u) : NULL;
        uint8_t *animation_table = readable_memory(component, 0xe0u) ?
            *(uint8_t **)(component + 0xdcu) : NULL;
        unsigned int mapped_animation_id = 0u;
        int mapped_lookup = -2;
        BOOL mapped_animation = FALSE;
        BOOL world_mapping_succeeded = FALSE;
        BOOL preserve_world_channel = FALSE;

        animation_ids[channel] = 0u;
        first_handles[channel] = 0u;
        second_handles[channel] = 0u;
        target_lookups[channel] = -2;
        source_lookups[channel] = -2;
        if (readable_memory(
                animation_state,
                channel * 4u + 3u) &&
            readable_memory(component, 0x134u) &&
            readable_memory(animation_table, 0x14u + 256u * 4u)) {
            unsigned int animation_id = animation_state[channel * 4u + 2u];
            uint8_t *animation_resource = *(uint8_t **)(
                animation_table + 0x14u + animation_id * 4u
            );

            animation_ids[channel] = animation_id;
            if (readable_memory(animation_resource, 0x28u)) {
                first_handles[channel] = *(uint32_t *)(animation_resource + 0x14u);
                second_handles[channel] = *(uint32_t *)(animation_resource + 0x20u);
                BOOL active_uses_first_bank =
                    (component[0x133u] & 2u) != 0u;
                int target_handle = *(int *)(animation_resource +
                    (active_uses_first_bank ? 0x20u : 0x14u));
                int source_handle = *(int *)(animation_resource +
                    (active_uses_first_bank ? 0x14u : 0x20u));
                int translated = target_handle != 0 &&
                    target_handle != 0x7ffff ?
                    world_lookup(world_interface, target_handle) : -1;

                if (translated != -1) {
                    target_lookups[channel] = translated;
                }

                if (translated == -1 && source_handle != 0 &&
                    source_handle != 0x7ffff) {
                    translated = world_lookup(
                        world_interface,
                        source_handle
                    );
                    source_lookups[channel] = translated;
                }
                if (translated != -1) {
                    selector = translated;
                    world_mapping_succeeded = TRUE;
                }
            }

            if (ailish_character) {
                mapped_animation_id = ailish_world_animation_id(animation_id);
                mapped_animation = mapped_animation_id != 0u &&
                    mapped_animation_id != animation_id;
                if (mapped_animation && readable_memory(
                        animation_table,
                        0x14u + (mapped_animation_id + 1u) * 4u)) {
                    uint8_t *mapped_resource = *(uint8_t **)(
                        animation_table + 0x14u + mapped_animation_id * 4u
                    );
                    if (readable_memory(mapped_resource, 0x28u)) {
                        BOOL native_uses_first_bank =
                            (component[0x133u] & 2u) != 0u;
                        int mapped_handle = *(int *)(mapped_resource +
                            (native_uses_first_bank ? 0x14u : 0x20u));

                        mapped_lookup = mapped_handle != 0 &&
                            mapped_handle != 0x7ffff ?
                            world_lookup(world_interface, mapped_handle) : -1;
                        if (mapped_lookup == -1) {
                            int alternate_handle = *(int *)(mapped_resource +
                                (native_uses_first_bank ? 0x20u : 0x14u));
                            if (alternate_handle != 0 &&
                                alternate_handle != 0x7ffff) {
                                mapped_lookup = world_lookup(
                                    world_interface,
                                    alternate_handle
                                );
                            }
                        }
                        if (mapped_lookup != -1) {
                            selector = mapped_lookup;
                            world_mapping_succeeded = TRUE;
                            SudekiMpLogFormat(
                                "split_screen_render event=ranged_world_animation_map character=0x%08lx source_id=%02x target_id=%02x world_selector=%d policy=ailish_observer_only_native_world_bank\r\n",
                                (unsigned long)(uintptr_t)character_pointer,
                                animation_id,
                                mapped_animation_id,
                                mapped_lookup
                            );
                        }
                    }
                }
            }
        }

        /*
         * Ailish's electric reload stages exist only in the first-person
         * authored bank on this build.  Copying their raw selectors into the
         * observer's world wrapper produces the visible idle/arms-only pose.
         * Keep the last native world channel instead until a corresponding
         * world action is confirmed; never invent a selector here.
         */
        preserve_world_channel = !target_is_first_person &&
            ailish_character &&
            ailish_first_person_only_animation(animation_ids[channel]) &&
            !world_mapping_succeeded;
        if (preserve_world_channel) {
            if (!ranged_translation_trace_valid[channel] ||
                ranged_translation_last_animation_id[channel] !=
                    animation_ids[channel]) {
                SudekiMpLogFormat(
                    "split_screen_render event=ranged_world_animation_fallback character=0x%08lx channel=%u source_id=%02x policy=observer_preserve_last_native_world_channel\r\n",
                    (unsigned long)(uintptr_t)character_pointer,
                    channel,
                    animation_ids[channel]
                );
            }
            ranged_translation_trace_valid[channel] = TRUE;
            ranged_translation_last_animation_id[channel] =
                animation_ids[channel];
            ranged_translation_last_bank =
                (component[0x133u] & 2u) != 0u;
            ranged_translation_last_first_handle[channel] =
                first_handles[channel];
            ranged_translation_last_second_handle[channel] =
                second_handles[channel];
            ranged_translation_last_target_lookup[channel] =
                target_lookups[channel];
            ranged_translation_last_source_lookup[channel] =
                source_lookups[channel];
            ranged_translation_last_selector[channel] =
                first_person_selector;
            selectors[channel] = first_person_selector;
            continue;
        }

        selectors[channel] = selector;
        if (!ranged_translation_trace_valid[channel] ||
            ranged_translation_last_animation_id[channel] !=
                animation_ids[channel] ||
            ranged_translation_last_bank !=
                ((component[0x133u] & 2u) != 0u) ||
            ranged_translation_last_first_handle[channel] !=
                first_handles[channel] ||
            ranged_translation_last_second_handle[channel] !=
                second_handles[channel] ||
            ranged_translation_last_target_lookup[channel] !=
                target_lookups[channel] ||
            ranged_translation_last_source_lookup[channel] !=
                source_lookups[channel] ||
            ranged_translation_last_selector[channel] != selector) {
            SudekiMpLogFormat(
                "split_screen_render event=ranged_animation_translation component=0x%08lx channel=%u animation_id=%02x bank=%s first_handle=0x%08lx second_handle=0x%08lx target_lookup=%d source_lookup=%d first_person_selector=%d chosen_world_selector=%d policy=diagnostic_only_no_native_resource_or_animation_write\r\n",
                (unsigned long)(uintptr_t)component,
                channel,
                animation_ids[channel],
                (component[0x133u] & 2u) != 0u ? "first" : "second",
                (unsigned long)first_handles[channel],
                (unsigned long)second_handles[channel],
                target_lookups[channel],
                source_lookups[channel],
                first_person_selector,
                selector
            );
            ranged_translation_trace_valid[channel] = TRUE;
            ranged_translation_last_animation_id[channel] =
                animation_ids[channel];
            ranged_translation_last_bank =
                (component[0x133u] & 2u) != 0u;
            ranged_translation_last_first_handle[channel] =
                first_handles[channel];
            ranged_translation_last_second_handle[channel] =
                second_handles[channel];
            ranged_translation_last_target_lookup[channel] =
                target_lookups[channel];
            ranged_translation_last_source_lookup[channel] =
                source_lookups[channel];
            ranged_translation_last_selector[channel] = selector;
        }
        if (!isfinite(rate) || !isfinite(time) || !isfinite(blend)) {
            continue;
        }
        for (submodel = 0u; submodel < world_submodels; ++submodel) {
            set_selector(
                world_interface,
                (int)channel,
                submodel,
                selector
            );
            set_state(
                world_interface,
                (int)channel,
                submodel,
                (state & 1u) != 0u
            );
            set_time(
                world_interface,
                (int)channel,
                submodel,
                time,
                0
            );
            set_rate(world_interface, (int)channel, submodel, rate);
        }
        set_blend(world_interface, (int)channel, blend);
    }
    if (*world_flags == visible_world_flags) {
        *world_flags = saved_world_flags;
    }

    if ((target_is_first_person &&
         !ranged_first_person_animation_mirror_logged) ||
        (!target_is_first_person &&
         !ranged_model_animation_mirror_logged)) {
        if (target_is_first_person) {
            ranged_first_person_animation_mirror_logged = TRUE;
        } else {
            ranged_model_animation_mirror_logged = TRUE;
        }
        SudekiMpLogFormat(
            "split_screen_render event=ranged_model_animation_mirror phase=active direction=%s source_wrapper=0x%08lx target_wrapper=0x%08lx source_submodels=%u target_submodels=%u animation_bank=%s animation_ids=%u,%u,%u,%u,%u target_selectors=%d,%d,%d,%d,%d policy=prefer_opposite_animation_bank_handle_for_target_model_then_copy_state_time_rate_and_blend_without_advancing_clock_or_events\r\n",
            target_is_first_person ? "world_to_first_person" :
                "first_person_to_world",
            (unsigned long)(uintptr_t)source_wrapper,
            (unsigned long)(uintptr_t)target_wrapper,
            first_person_submodels,
            world_submodels,
            (component[0x133u] & 2u) != 0u ? "first" : "second",
            animation_ids[0],
            animation_ids[1],
            animation_ids[2],
            animation_ids[3],
            animation_ids[4],
            selectors[0],
            selectors[1],
            selectors[2],
            selectors[3],
            selectors[4]
        );
    }
    return TRUE;
}

static BOOL restore_ranged_model_render_view(void) {
    BOOL restored = TRUE;
    unsigned int index;

    for (index = RANGED_MODEL_RENDER_SWAP_COUNT; index > 0u; --index) {
        SudekiMpRangedModelRenderSwap swap =
            ranged_model_render_swaps[index - 1u];
        BOOL attachment_restored = FALSE;

        ZeroMemory(
            &ranged_model_render_swaps[index - 1u],
            sizeof(ranged_model_render_swaps[index - 1u])
        );
        if (!swap.active) {
            continue;
        }
        if (!writable_memory(
                swap.attachment_slot,
                sizeof(*swap.attachment_slot))) {
            restored = FALSE;
        } else if (*swap.attachment_slot == swap.applied_attachment) {
            *swap.attachment_slot = swap.saved_attachment;
            attachment_restored = TRUE;
        } else if (*swap.attachment_slot == swap.saved_attachment) {
            attachment_restored = TRUE;
        } else if (*swap.attachment_slot != swap.saved_attachment) {
            restored = FALSE;
        }
        if (swap.weapon_reattached &&
            (!attachment_restored ||
             !reattach_ranged_weapon_to_current_model(
                 swap.weapon,
                 "owner_first_person_restore"))) {
            restored = FALSE;
        }
        if (!writable_memory(
                swap.first_person_flags,
                sizeof(*swap.first_person_flags))) {
            restored = FALSE;
        } else if (*swap.first_person_flags ==
                   swap.applied_first_person_flags) {
            *swap.first_person_flags = swap.saved_first_person_flags;
        } else if (*swap.first_person_flags !=
                   swap.saved_first_person_flags) {
            restored = FALSE;
        }
        if (!writable_memory(
                swap.world_flags,
                sizeof(*swap.world_flags))) {
            restored = FALSE;
        } else if (*swap.world_flags == swap.applied_world_flags) {
            *swap.world_flags = swap.saved_world_flags;
        } else if (*swap.world_flags != swap.saved_world_flags) {
            restored = FALSE;
        }
        log_ranged_transform_trace("owner_restored", &swap);
    }
    return restored;
}

static BOOL stage_ranged_model_render_view(
    void *character_pointer,
    BOOL show_first_person,
    const char *ownership,
    BOOL *world_compositor_owned_result
) {
    uint8_t *character = (uint8_t *)character_pointer;
    uint8_t *position;
    uint8_t *component;
    void *first_person_wrapper;
    void *world_wrapper;
    void *first_person_render_object;
    void *world_render_object;
    void *weapon;
    void **attachment_slot;
    uint32_t *first_person_flags;
    uint32_t *world_flags;
    uint32_t saved_first_person_flags;
    uint32_t saved_world_flags;
    uint32_t applied_first_person_flags;
    uint32_t applied_world_flags;
    SudekiMpRangedModelRenderSwap *swap = NULL;
    BOOL world_compositor_eligible;
    BOOL world_compositor_applied;
    unsigned int index;

    if (world_compositor_owned_result != NULL) {
        *world_compositor_owned_result = FALSE;
    }

    if (!ranged_presentation_parts(
            character,
            &position,
            &component,
            &first_person_wrapper,
            &world_wrapper) ||
        !writable_memory(position, 0xb8u)) {
        return FALSE;
    }
    first_person_render_object = *(void **)(
        (uint8_t *)first_person_wrapper + 0x08u
    );
    world_render_object = *(void **)((uint8_t *)world_wrapper + 0x08u);
    if (!writable_memory(first_person_render_object, 0x38u) ||
        !writable_memory(world_render_object, 0x38u)) {
        return FALSE;
    }
    attachment_slot = (void **)(position + 0xb4u);
    if (*attachment_slot != (show_first_person ?
            world_wrapper : first_person_wrapper)) {
        return FALSE;
    }
    if (!show_first_person &&
        character_has_resource_type(character, 0x01u)) {
        log_ranged_state_details_inventory(
            character,
            component,
            first_person_wrapper,
            world_wrapper
        );
    }
    world_compositor_eligible = ranged_world_compositor_eligible(
        character,
        position,
        component,
        first_person_wrapper,
        world_wrapper,
        show_first_person
    );
    if (world_compositor_owned_result != NULL &&
        world_compositor_eligible) {
        *world_compositor_owned_result = TRUE;
    }
    world_compositor_applied = world_compositor_eligible &&
        compose_ranged_world_animation(
            character,
            component,
            first_person_wrapper,
            world_wrapper
        );
    if (!world_compositor_applied &&
        !sync_ranged_animation_model(
            character,
            component,
            (uint8_t *)(show_first_person ?
                world_wrapper : first_person_wrapper),
            (uint8_t *)(show_first_person ?
                first_person_wrapper : world_wrapper),
            show_first_person)) {
        return FALSE;
    }
    first_person_flags = (uint32_t *)(
        (uint8_t *)first_person_render_object + 0x34u
    );
    world_flags = (uint32_t *)((uint8_t *)world_render_object + 0x34u);
    saved_first_person_flags = *first_person_flags;
    saved_world_flags = *world_flags;
    applied_first_person_flags = show_first_person ?
        (saved_first_person_flags & ~RENDER_OBJECT_HIDDEN_FLAG) :
        (saved_first_person_flags | RENDER_OBJECT_HIDDEN_FLAG);
    applied_world_flags = show_first_person ?
        (saved_world_flags | RENDER_OBJECT_HIDDEN_FLAG) :
        (saved_world_flags & ~RENDER_OBJECT_HIDDEN_FLAG);

    for (index = 0u; index < RANGED_MODEL_RENDER_SWAP_COUNT; ++index) {
        if (!ranged_model_render_swaps[index].active) {
            swap = &ranged_model_render_swaps[index];
            break;
        }
    }
    if (swap == NULL) {
        return FALSE;
    }
    swap->attachment_slot = attachment_slot;
    swap->character = character;
    swap->position = position;
    swap->component = component;
    swap->first_person_render_object = first_person_render_object;
    swap->world_render_object = world_render_object;
    swap->applied_attachment = show_first_person ?
        first_person_wrapper : world_wrapper;
    swap->saved_attachment = *attachment_slot;
    swap->first_person_flags = first_person_flags;
    swap->applied_first_person_flags = applied_first_person_flags;
    swap->saved_first_person_flags = saved_first_person_flags;
    swap->world_flags = world_flags;
    swap->applied_world_flags = applied_world_flags;
    swap->saved_world_flags = saved_world_flags;
    swap->weapon = *(void **)(character + 0xc0u);
    swap->weapon_reattached = FALSE;
    if (!show_first_person &&
        (DWORD)(GetTickCount() - ranged_transform_trace_last_tick) >= 250u) {
        ranged_transform_trace_last_tick = GetTickCount();
        swap->transform_trace_sample = TRUE;
        swap->transform_trace_sequence =
            ++ranged_transform_trace_sequence;
        if (readable_memory(swap->weapon, 0xb0u)) {
            memcpy(
                swap->saved_weapon_local_matrix,
                (uint8_t *)swap->weapon + 0x70u,
                sizeof(swap->saved_weapon_local_matrix)
            );
            swap->saved_weapon_local_matrix_valid = TRUE;
        }
    }
    swap->active = TRUE;

    log_ranged_transform_trace("native_pre_stage", swap);
    *first_person_flags = applied_first_person_flags;
    *world_flags = applied_world_flags;
    *attachment_slot = swap->applied_attachment;
    weapon = swap->weapon;
    if (!show_first_person &&
        !reattach_ranged_weapon_to_current_model(
            weapon,
            "observer_world_stage")) {
        *attachment_slot = swap->saved_attachment;
        *first_person_flags = swap->saved_first_person_flags;
        *world_flags = swap->saved_world_flags;
        ZeroMemory(swap, sizeof(*swap));
        return FALSE;
    }
    swap->weapon_reattached = !show_first_person &&
        readable_memory(weapon, 0xf8u) &&
        *(void **)((uint8_t *)weapon + 0xf4u) != NULL;
    log_ranged_transform_trace("observer_world_staged", swap);
    if ((show_first_person &&
         !ranged_first_person_model_isolation_logged) ||
        (!show_first_person && !ranged_model_isolation_logged)) {
        if (show_first_person) {
            ranged_first_person_model_isolation_logged = TRUE;
        } else {
            ranged_model_isolation_logged = TRUE;
        }
        SudekiMpLogFormat(
            "split_screen_render event=ranged_model_isolation phase=active direction=%s ownership=%s character=0x%08lx component=0x%08lx position=0x%08lx first_person_wrapper=0x%08lx world_wrapper=0x%08lx weapon=0x%08lx weapon_reattached=%s saved_first_person_flags=0x%08lx saved_world_flags=0x%08lx policy=per_viewport_render_only_body_and_native_weapon_attachment_visibility_animation_borrow_restore_before_frame_end\r\n",
            show_first_person ? "world_to_first_person" :
                "first_person_to_world",
            ownership,
            (unsigned long)(uintptr_t)character,
            (unsigned long)(uintptr_t)component,
            (unsigned long)(uintptr_t)position,
            (unsigned long)(uintptr_t)first_person_wrapper,
            (unsigned long)(uintptr_t)world_wrapper,
            (unsigned long)(uintptr_t)weapon,
            swap->weapon_reattached ? "true" : "false",
            (unsigned long)saved_first_person_flags,
            (unsigned long)saved_world_flags
        );
    }
    return TRUE;
}

/* Camera 2's native collision solver is intentionally allowed to move the
 * eye closer than the authored third-person radius.  Sudeki's native local
 * body suppression is global/P1-oriented, so it does not hide Ailish in the
 * render-only Camera 2 pass.  Do the smallest possible equivalent here:
 * temporarily hide only her world render object.  This never swaps model
 * attachments, wrappers, weapons, animation, or camera state. */
static void stage_player_two_collision_self_cull(void) {
    void *world_wrapper;
    void *world_render_object;
    uint32_t *world_flags;
    uint32_t saved_world_flags;
    float actor_position[3];
    const float *matrix;
    float dx;
    float dy;
    float dz;
    float distance_squared;
    float radius_squared;

    if (fixed_three_assignment_selected() ||
        player_two_collision_self_cull.active ||
        !rendered_player_two_this_frame ||
        !native_second_player_camera_collision_session_enabled ||
        !player_two_native_camera_ready ||
        player_two_character == NULL || player_two_render_state == NULL ||
        !render_state_camera_matrix_valid(player_two_render_state) ||
        !character_position(player_two_character, actor_position) ||
        !ranged_presentation_parts(
            player_two_character, NULL, NULL, NULL, &world_wrapper) ||
        !readable_memory(world_wrapper, 0x0cu)) {
        return;
    }
    world_render_object = *(void **)((uint8_t *)world_wrapper + 0x08u);
    if (!writable_memory(world_render_object, 0x38u)) {
        return;
    }
    matrix = (const float *)((const uint8_t *)player_two_render_state +
        0x90u);
    dx = matrix[12] - actor_position[0];
    dy = matrix[13] - actor_position[1];
    dz = matrix[14] - actor_position[2];
    distance_squared = dx * dx + dy * dy + dz * dz;
    radius_squared = player_two_collision_self_cull_radius *
        player_two_collision_self_cull_radius;
    if (!isfinite(distance_squared) || distance_squared >= radius_squared) {
        return;
    }
    world_flags = (uint32_t *)((uint8_t *)world_render_object + 0x34u);
    saved_world_flags = *world_flags;
    player_two_collision_self_cull.active = TRUE;
    player_two_collision_self_cull.character = player_two_character;
    player_two_collision_self_cull.world_render_object = world_render_object;
    player_two_collision_self_cull.world_flags = world_flags;
    player_two_collision_self_cull.saved_world_flags = saved_world_flags;
    player_two_collision_self_cull.applied_world_flags = saved_world_flags |
        RENDER_OBJECT_HIDDEN_FLAG;
    *world_flags = player_two_collision_self_cull.applied_world_flags;
    if (!player_two_collision_self_cull_logged) {
        player_two_collision_self_cull_logged = TRUE;
        SudekiMpLogFormat(
            "split_screen_render event=player_two_collision_self_cull "
            "phase=active character=0x%08lx render_object=0x%08lx "
            "radius_bits=0x%08lx policy=hide_only_camera_two_local_world_body_restore_before_native_frame_end\\r\\n",
            (unsigned long)(uintptr_t)player_two_character,
            (unsigned long)(uintptr_t)world_render_object,
            (unsigned long)float_bits(player_two_collision_self_cull_radius));
    }
}

static void restore_player_two_collision_self_cull(void) {
    SudekiMpPlayerTwoCollisionSelfCull swap = player_two_collision_self_cull;

    if (!swap.active) {
        return;
    }
    ZeroMemory(&player_two_collision_self_cull,
        sizeof(player_two_collision_self_cull));
    if (!writable_memory(swap.world_flags, sizeof(*swap.world_flags))) {
        return;
    }
    if (*swap.world_flags == swap.applied_world_flags) {
        *swap.world_flags = swap.saved_world_flags;
    }
}

static void apply_ranged_model_render_view(void) {
    uint8_t *arbiter;
    BOOL world_compositor_owned = FALSE;

    if (fixed_three_assignment_selected() ||
        !rendered_player_two_this_frame) {
        return;
    }
    stage_player_two_collision_self_cull();
    if (!split_screen_ranged_model_isolation_enabled) {
        return;
    }
    if (readable_memory(player_one_character, 0x94u)) {
        arbiter = *(uint8_t **)(
            (uint8_t *)player_one_character + 0x90u
        );
        if (readable_memory(arbiter, 0x54u) &&
            (*(uint32_t *)(arbiter + 0x50u) &
             RANGED_FIRST_PERSON_ARBITER_FLAG) != 0u) {
            stage_ranged_model_render_view(
                player_one_character,
                FALSE,
                "player_one_observed_by_player_two",
                &world_compositor_owned
            );
        }
    }
    if (player_two_should_use_first_person()) {
        stage_ranged_model_render_view(
            player_two_character,
            TRUE,
            "player_two_own_combat_view",
            NULL
        );
    }
    if (!world_compositor_owned) {
        reset_ranged_world_compositor("observer_ownership_inactive");
    }
}

static BOOL restore_render_only_camera(void) {
    void **slot;
    void *applied_state;
    void *original_state;

    if (!render_only_swap_active) {
        return TRUE;
    }
    slot = render_only_camera_slot;
    applied_state = render_only_applied_state;
    original_state = render_only_original_state;
    if (!readable_memory(slot, sizeof(*slot))) {
        log_second_player_camera_rejection_once(
            12u,
            "scene_render_slot_unavailable_during_restore"
        );
        return FALSE;
    }
    if (*slot == original_state) {
        render_only_swap_active = FALSE;
        render_only_camera_slot = NULL;
        render_only_applied_state = NULL;
        render_only_original_state = NULL;
        return TRUE;
    }
    if (*slot == applied_state) {
        if (!writable_memory(slot, sizeof(*slot))) {
            log_second_player_camera_rejection_once(
                12u,
                "scene_render_slot_not_writable_during_restore"
            );
            return FALSE;
        }
        *slot = original_state;
    }
    if (*slot != original_state) {
        log_second_player_camera_rejection_once(
            13u,
            "scene_render_owner_changed_during_frame"
        );
        return FALSE;
    }
    render_only_swap_active = FALSE;
    render_only_camera_slot = NULL;
    render_only_applied_state = NULL;
    render_only_original_state = NULL;
    return TRUE;
}

static void apply_render_only_camera(void) {
    void **slot;
    void *native_camera;
    void *native_render_state;
    void *desired_render_state;

    if (!second_player_camera_enabled ||
        player_two_camera == NULL) {
        return;
    }
    native_camera = current_render_camera(second_player_camera_manager);
    if (!readable_memory(native_camera, 0x38u)) {
        log_second_player_camera_rejection_once(
            14u,
            "global_render_camera_unavailable"
        );
        player_two_view_requested = FALSE;
        return;
    }
    native_render_state = *(void **)((uint8_t *)native_camera + 0x34u);
    slot = current_scene_render_camera_slot();
    if (!readable_memory(native_render_state, 0xdcu) || slot == NULL ||
        *slot != native_render_state) {
        log_second_player_camera_rejection_once(
            15u,
            "scene_render_owner_not_native_camera"
        );
        player_two_view_requested = FALSE;
        return;
    }
    desired_render_state = player_skill_render_states[
        player_two_view_requested ? 1 : 0
    ];
    if (desired_render_state == NULL && player_two_view_requested &&
        !update_player_two_render_state()) {
        log_second_player_camera_rejection_once(
            16u,
            "translated_render_state_update_failed"
        );
        player_two_view_requested = FALSE;
        return;
    }
    if (desired_render_state == NULL) {
        desired_render_state = player_two_view_requested ?
            player_two_render_state : native_render_state;
    }
    if (desired_render_state == native_render_state) {
        second_player_camera_last_rejection = 0u;
        return;
    }
    *slot = desired_render_state;
    render_only_camera_slot = slot;
    render_only_applied_state = desired_render_state;
    render_only_original_state = native_render_state;
    render_only_swap_active = TRUE;
    second_player_camera_last_rejection = 0u;
}

static BOOL render_only_camera_phase_confirmed(BOOL player_two_expected) {
    void *native_camera;
    void *native_render_state;
    void **slot;

    if (!readable_memory(second_player_camera_manager, 0x4cu)) {
        return FALSE;
    }
    native_camera = current_render_camera(second_player_camera_manager);
    if (!readable_memory(native_camera, 0x38u)) {
        return FALSE;
    }
    native_render_state = *(void **)((uint8_t *)native_camera + 0x34u);
    slot = current_scene_render_camera_slot();
    if (!readable_memory(native_render_state, 0xdcu) ||
        slot == NULL ||
        !readable_memory(slot, sizeof(*slot))) {
        return FALSE;
    }
    if (!player_two_expected) {
        return !render_only_swap_active && *slot == native_render_state;
    }
    return render_only_swap_active &&
        render_only_camera_slot == slot &&
        render_only_original_state == native_render_state &&
        render_only_applied_state != native_render_state &&
        *slot == render_only_applied_state;
}

static void invalidate_dual_frame_cache(void) {
    player_one_frame_valid = FALSE;
    player_two_frame_valid = FALSE;
    dual_compositor_logged = FALSE;
}

static void swap_dual_frame_cache_ownership(void) {
    void *surface = player_one_frame_surface;
    void *texture = player_one_frame_texture;
    BOOL valid = player_one_frame_valid;

    player_one_frame_surface = player_two_frame_surface;
    player_one_frame_texture = player_two_frame_texture;
    player_one_frame_valid = player_two_frame_valid;
    player_two_frame_surface = surface;
    player_two_frame_texture = texture;
    player_two_frame_valid = valid;
}

static void clear_player_two_camera_runtime_state(void) {
    second_player_camera_manager = NULL;
    player_one_camera = NULL;
    player_two_camera = NULL;
    player_one_render_state = NULL;
    player_two_render_state = NULL;
    player_skill_cameras[0] = NULL;
    player_skill_cameras[1] = NULL;
    player_skill_render_states[0] = NULL;
    player_skill_render_states[1] = NULL;
    skill_camera_history_tail_until = 0u;
    player_one_character = NULL;
    player_two_character = NULL;
    reset_player_two_controller_camera();
    reset_ranged_animation_trace();
    reset_hud_mapping_trace();
    reset_minimap_owner_schedule();
    SudekiMpCombatContextSetView(0u, NULL, NULL);
    SudekiMpCombatContextSetView(1u, NULL, NULL);
    player_two_party_slot = 0u;
    player_two_view_requested = FALSE;
    rendered_player_two_this_frame = FALSE;
    viewport_hud_binding_active = FALSE;
    invalidate_dual_frame_cache();
}

static BOOL release_player_two_camera(const char *reason) {
    void *manager = second_player_camera_manager;
    void *live_manager = NULL;
    void *named_camera = NULL;
    void *global_camera = NULL;
    void *global_render_state = NULL;
    void *scene_render_state = NULL;
    void **scene_slot = NULL;
    unsigned int release_decision;
    BOOL manager_matches;
    BOOL named_identity_known = FALSE;
    BOOL global_identity_known = FALSE;
    BOOL scene_identity_known = FALSE;

    restore_player_two_collision_self_cull();
    restore_ranged_model_render_view();
    reset_ranged_world_compositor("camera_release");
    if (!restore_render_only_camera()) {
        return FALSE;
    }
    if (player_two_camera == NULL) {
        clear_player_two_camera_runtime_state();
        return TRUE;
    }
    if (game_base == NULL || !readable_memory(
            game_base + RVA_CAMERA_MANAGER_GLOBAL,
            sizeof(live_manager))) {
        log_second_player_camera_rejection_once(
            8u,
            "camera_manager_identity_unavailable_during_release"
        );
        return FALSE;
    }
    live_manager = *(void **)(game_base + RVA_CAMERA_MANAGER_GLOBAL);
    manager_matches = live_manager != NULL && live_manager == manager;
    if (readable_memory(live_manager, 0x4cu)) {
        global_camera = current_render_camera(live_manager);
        if (readable_memory(global_camera, 0x38u)) {
            global_render_state = *(void **)((uint8_t *)global_camera +
                CAMERA_RENDER_STATE_OFFSET);
            global_identity_known = TRUE;
        }
    }
    scene_slot = current_scene_render_camera_slot();
    if (scene_slot != NULL && readable_memory(scene_slot, sizeof(*scene_slot))) {
        scene_render_state = *scene_slot;
        scene_identity_known = TRUE;
    }
    if (manager_matches && readable_memory(manager, 0x4cu) &&
        camera_manager_get_camera != NULL) {
        named_camera = camera_manager_get_camera(
            manager, second_player_camera_name);
        named_identity_known = TRUE;
    }
    release_decision = SudekiMpSplitScreenNativeCameraReleasePolicy(
        manager_matches,
        named_identity_known,
        named_camera,
        player_two_camera,
        global_identity_known,
        global_camera,
        global_render_state,
        player_one_camera,
        player_one_render_state,
        scene_identity_known,
        scene_render_state,
        player_two_render_state,
        !render_only_swap_active);
    if (release_decision == SUDEKIMP_NATIVE_CAMERA_RELEASE_WAIT) {
        log_second_player_camera_rejection_once(
            11u,
            "camera_release_ownership_not_safe_or_unknown"
        );
        if (player_two_native_camera_stage !=
                SUDEKIMP_NATIVE_CAMERA_STAGE_IDLE) {
            request_native_camera_manual_recreation(
                "release_ownership_not_safe_or_unknown");
        }
        return FALSE;
    }
    if (release_decision == SUDEKIMP_NATIVE_CAMERA_RELEASE_ABANDON) {
        SudekiMpLogFormat(
            "split_screen_render event=second_player_camera phase=release "
            "reason=%s status=abandoned_unowned_generation_or_name "
            "saved_manager=0x%08lx live_manager=0x%08lx "
            "saved_camera=0x%08lx named_camera=0x%08lx "
            "global_camera=0x%08lx global_render_state=0x%08lx "
            "scene_render_state=0x%08lx "
            "policy=clear_only_after_live_global_and_scene_exclude_saved_player_two_ownership\r\n",
            reason == NULL ? "unspecified" : reason,
            (unsigned long)(uintptr_t)manager,
            (unsigned long)(uintptr_t)live_manager,
            (unsigned long)(uintptr_t)player_two_camera,
            (unsigned long)(uintptr_t)named_camera,
            (unsigned long)(uintptr_t)global_camera,
            (unsigned long)(uintptr_t)global_render_state,
            (unsigned long)(uintptr_t)scene_render_state);
        clear_player_two_camera_runtime_state();
        return TRUE;
    }

    if (camera_manager_get_camera == NULL ||
        camera_manager_remove_camera == NULL ||
        !readable_memory(manager, 0x4cu) ||
        !readable_memory(
            game_base + RVA_CAMERA_MANAGER_GLOBAL,
            sizeof(live_manager))) {
        log_second_player_camera_rejection_once(
            11u,
            "camera_manager_api_or_identity_unavailable_before_remove"
        );
        return FALSE;
    }
    live_manager = *(void **)(game_base + RVA_CAMERA_MANAGER_GLOBAL);
    global_camera = current_render_camera(manager);
    if (!readable_memory(global_camera, 0x38u)) {
        log_second_player_camera_rejection_once(
            9u,
            "global_camera_unreadable_before_remove"
        );
        return FALSE;
    }
    global_render_state = *(void **)((uint8_t *)global_camera +
        CAMERA_RENDER_STATE_OFFSET);
    scene_slot = current_scene_render_camera_slot();
    if (live_manager != manager || global_camera != player_one_camera ||
        global_render_state != player_one_render_state ||
        scene_slot == NULL || !readable_memory(scene_slot, sizeof(*scene_slot)) ||
        *scene_slot != player_one_render_state || render_only_swap_active) {
        log_second_player_camera_rejection_once(
            9u,
            "exact_player_one_ownership_changed_before_remove"
        );
        return FALSE;
    }
    if (!readable_memory(
            game_base + RVA_CAMERA_MANAGER_GLOBAL,
            sizeof(live_manager)) ||
        *(void **)(game_base + RVA_CAMERA_MANAGER_GLOBAL) != manager) {
        log_second_player_camera_rejection_once(
            9u,
            "camera_manager_generation_changed_before_remove"
        );
        return FALSE;
    }
    /* Keep this identity read immediately adjacent to RemoveCamera. A reused
     * name is never removed, even when all other release ownership is safe. */
    named_camera = camera_manager_get_camera(
        manager, second_player_camera_name);
    if (named_camera != player_two_camera) {
        SudekiMpLogFormat(
            "split_screen_render event=second_player_camera phase=release "
            "reason=%s status=abandoned_name_changed_before_remove "
            "saved_camera=0x%08lx named_camera=0x%08lx\r\n",
            reason == NULL ? "unspecified" : reason,
            (unsigned long)(uintptr_t)player_two_camera,
            (unsigned long)(uintptr_t)named_camera);
        clear_player_two_camera_runtime_state();
        return TRUE;
    }
    camera_manager_remove_camera(manager, second_player_camera_name);
    named_camera = camera_manager_get_camera(
        manager, second_player_camera_name);
    if (named_camera == player_two_camera) {
        log_second_player_camera_rejection_once(
            10u,
            "camera_manager_retained_saved_player_two_camera"
        );
        return FALSE;
    }
    SudekiMpLogFormat(
        "split_screen_render event=second_player_camera phase=release "
        "reason=%s status=removed_saved_camera post_named_camera=0x%08lx\r\n",
        reason == NULL ? "unspecified" : reason,
        (unsigned long)(uintptr_t)named_camera
    );
    clear_player_two_camera_runtime_state();
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
    reset_player_two_controller_camera();
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
    SudekiMpCombatContextSetView(
        0u,
        player_one_camera,
        player_one_render_state
    );
    SudekiMpCombatContextSetView(
        1u,
        player_two_camera,
        player_two_render_state
    );
    reset_ranged_animation_trace();
    reset_hud_mapping_trace();
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

static BOOL rebind_player_characters(
    void *first_character,
    void *second_character,
    unsigned int second_slot
) {
    void *old_first_character = player_one_character;
    void *old_second_character = player_two_character;
    unsigned int old_second_slot = player_two_party_slot;
    BOOL reversed = first_character == old_second_character &&
        second_character == old_first_character;

    player_one_character = first_character;
    player_two_character = second_character;
    player_two_party_slot = second_slot;
    reset_player_two_controller_camera();
    if (!update_player_two_render_state()) {
        player_one_character = old_first_character;
        player_two_character = old_second_character;
        player_two_party_slot = old_second_slot;
        reset_player_two_controller_camera();
        return FALSE;
    }
    if (dual_camera_frame_cache_enabled) {
        if (reversed) {
            swap_dual_frame_cache_ownership();
        } else {
            invalidate_dual_frame_cache();
        }
    }
    reset_ranged_animation_trace();
    reset_hud_mapping_trace();
    reset_minimap_owner_schedule();
    reset_ranged_world_compositor("player_ownership_rebind");
    SudekiMpLogFormat(
        "split_screen_render event=player_ownership phase=rebind old_player_one=0x%08lx old_player_two=0x%08lx player_one=0x%08lx player_two=0x%08lx player_two_party_slot=%u cache_policy=%s reason=controller_owned_character_changed\r\n",
        (unsigned long)(uintptr_t)old_first_character,
        (unsigned long)(uintptr_t)old_second_character,
        (unsigned long)(uintptr_t)player_one_character,
        (unsigned long)(uintptr_t)player_two_character,
        player_two_party_slot,
        reversed && dual_camera_frame_cache_enabled ?
            "swap_existing_player_frames" : "invalidate_and_refill"
    );
    return TRUE;
}

static void poll_second_player_camera(BOOL gameplay_allowed) {
    BOOL key_is_down;
    BOOL settled_temporary;
    void *first_character;
    void *second_character;
    unsigned int second_slot;

    if (!second_player_camera_enabled) {
        return;
    }
    key_is_down =
        (GetAsyncKeyState((int)second_player_camera_virtual_key) & 0x8000) != 0;
    if (player_two_native_camera_recreate_requested) {
        if (!release_player_two_camera("native_camera_session_fallback")) {
            player_two_view_requested = FALSE;
            second_player_camera_key_was_down = key_is_down;
            return;
        }
        player_two_native_camera_recreate_requested = FALSE;
        SudekiMpLogWrite(
            "split_screen_render event=player_two_native_camera "
            "phase=manual_recreation_boundary status=released "
            "policy=next_named_camera_uses_manual_translated_orbit_only\r\n");
    }
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
            if (!rebind_player_characters(
                    first_character,
                    second_character,
                    second_slot)) {
                release_player_two_camera("player_rebind_failed");
                second_player_camera_key_was_down = key_is_down;
                return;
            }
        } else {
            release_player_two_camera("party_assignment_changed");
            second_player_camera_key_was_down = key_is_down;
            return;
        }
    }
    /* A shared full-width TEMP frame no longer schedules Camera 2, so service
     * the policy explicitly until the room exits or the native P2 Exploration
     * camera proves ready. */
    settled_temporary = settled_temporary_zone_active();
    if ((settled_temporary ||
         player_two_temporary_camera_policy !=
            SUDEKIMP_TEMP_CAMERA_OUTSIDE) &&
        !update_player_two_render_state()) {
        if (settled_temporary) {
            set_player_two_temporary_camera_policy(
                SUDEKIMP_TEMP_CAMERA_SHARED_FULL_WIDTH,
                current_player_one_camera_mode(),
                "temporary_render_state_update_unavailable");
        }
        player_two_view_requested = FALSE;
        second_player_camera_key_was_down = key_is_down;
        return;
    }
    if (player_two_temporary_camera_policy ==
            SUDEKIMP_TEMP_CAMERA_SHARED_FULL_WIDTH) {
        player_two_view_requested = FALSE;
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

static BOOL gameplay_split_allowed(
    BOOL runtime_authorized,
    const char **reason
) {
    uint8_t *group;
    uint8_t *controller;
    uint8_t *camera_mode;
    void *front_character;
    void *controller_target;
    void *camera_pointer;

    if (SudekiMpSplitScreenNativeMovieActive()) {
        *reason = "native_movie_active";
        return FALSE;
    }
    if (!runtime_authorized) {
        *reason = runtime_split_enabled ?
            "external_runtime_authorization_pending" :
            "runtime_toggle_disabled";
        return FALSE;
    }
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

static void log_quick_menu_gate(BOOL visible, BOOL live_player_two_ready) {
    int state = visible ? (live_player_two_ready ? 2 : 1) : 0;

    if (state == quick_menu_gate_last_state) {
        return;
    }
    quick_menu_gate_last_state = state;
    SudekiMpLogFormat(
        "split_screen_render event=quick_menu_gate state=%s isolation_mode=%s policy=pin_native_menu_owner_cache_for_current_and_queued_ui_preserve_non_owner_clean_cache\r\n",
        visible ? "active" : "inactive",
        visible ? (live_player_two_ready ? "owner_pinned" :
            "frozen_cache_fallback") : "not_applicable"
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

static BOOL fixed_three_assignment_selected(void) {
    SudekiMpCoopRosterAssignment assignment;

    return coop_roster_valid &&
        SudekiMpCoopRosterAssignmentStoreGet(
            &coop_roster_assignment_store, &assignment) &&
        assignment.active_human_mask == FIXED_THREE_HUMAN_MASK;
}

static void fixed_three_reset_render_owner_evidence(void) {
    fixed_three_runtime.hud_evidence_seat = UINT_MAX;
    fixed_three_runtime.hud_role_mask = 0u;
    fixed_three_runtime.hud_source_failure_this_frame = FALSE;
    fixed_three_runtime.portrait_evidence_seat = UINT_MAX;
    fixed_three_runtime.portrait_role_mask = 0u;
    fixed_three_runtime.minimap_render_seat =
        SUDEKIMP_LOCAL_VIEWPORT_NO_CONTROLLER;
    fixed_three_runtime.minimap_render_valid = FALSE;
}

static void fixed_three_clear_minimap_update_proof(void) {
    fixed_three_runtime.minimap_update_seat =
        SUDEKIMP_LOCAL_VIEWPORT_NO_CONTROLLER;
    fixed_three_runtime.minimap_update_epoch = 0u;
    fixed_three_runtime.minimap_update_valid = FALSE;
    minimap_source_failure_this_frame = FALSE;
}

static void fixed_three_arm_minimap_update(unsigned int seat_index) {
    fixed_three_clear_minimap_update_proof();
    if (seat_index >= FIXED_THREE_SEAT_COUNT) {
        fixed_three_runtime.minimap_expected_update_seat =
            SUDEKIMP_LOCAL_VIEWPORT_NO_CONTROLLER;
        return;
    }
    ++fixed_three_runtime.minimap_expected_update_epoch;
    if (fixed_three_runtime.minimap_expected_update_epoch == 0u) {
        ++fixed_three_runtime.minimap_expected_update_epoch;
    }
    fixed_three_runtime.minimap_expected_update_seat = (uint8_t)seat_index;
}

static BOOL fixed_three_consume_frame_owner_evidence(
    unsigned int rendered_seat
) {
    BOOL ready = SudekiMpSplitScreenFixedThreeFrameOwnerEvidencePolicy(
        rendered_seat,
        fixed_three_runtime.hud_evidence_seat,
        fixed_three_runtime.hud_role_mask,
        fixed_three_runtime.portrait_evidence_seat,
        fixed_three_runtime.portrait_role_mask,
        fixed_three_runtime.minimap_expected_update_epoch,
        fixed_three_runtime.minimap_update_valid,
        fixed_three_runtime.minimap_update_seat,
        fixed_three_runtime.minimap_update_epoch,
        fixed_three_runtime.minimap_render_valid,
        fixed_three_runtime.minimap_render_seat,
        fixed_three_runtime.hud_source_failure_this_frame ||
            minimap_source_failure_this_frame);

    fixed_three_reset_render_owner_evidence();
    fixed_three_clear_minimap_update_proof();
    fixed_three_runtime.minimap_expected_update_seat =
        SUDEKIMP_LOCAL_VIEWPORT_NO_CONTROLLER;
    return ready;
}

static void fixed_three_invalidate_frame_cache(void) {
    fixed_three_runtime.frame_valid_mask = 0u;
    fixed_three_runtime.frame_owner_evidence_mask = 0u;
    fixed_three_runtime.compositor_logged = FALSE;
    fixed_three_reset_render_owner_evidence();
    fixed_three_clear_minimap_update_proof();
    fixed_three_runtime.minimap_expected_update_seat =
        SUDEKIMP_LOCAL_VIEWPORT_NO_CONTROLLER;
    ZeroMemory(&fixed_three_runtime.layout,
        sizeof(fixed_three_runtime.layout));
}

static void fixed_three_release_frame_surfaces(void) {
    unsigned int seat_index;

    for (seat_index = FIXED_THREE_SEAT_COUNT; seat_index > 0u;
            --seat_index) {
        release_com_object(
            &fixed_three_runtime.frame_surfaces[seat_index - 1u]);
    }
    for (seat_index = FIXED_THREE_SEAT_COUNT; seat_index > 0u;
            --seat_index) {
        release_com_object(
            &fixed_three_runtime.frame_textures[seat_index - 1u]);
    }
    fixed_three_runtime.frame_device = NULL;
    ZeroMemory(&fixed_three_runtime.frame_description,
        sizeof(fixed_three_runtime.frame_description));
    fixed_three_invalidate_frame_cache();
}

static BOOL fixed_three_collect_leases(
    SudekiMpFixedThreeSeatLeaseCandidate *candidate
) {
    SudekiMpCoopRosterAssignment assignment;
    SudekiMpPlayerStatehood *statehood = SudekiMpPlayerStatehoodRuntime();
    uint8_t *group;
    uint8_t *controller;
    void *controller_target;
    unsigned int seat_index;
    unsigned int party_index;

    if (candidate == NULL || game_base == NULL || statehood == NULL ||
        !SudekiMpCoopRosterAssignmentStoreGet(
            &coop_roster_assignment_store, &assignment) ||
        assignment.active_human_mask != FIXED_THREE_HUMAN_MASK ||
        SudekiMpLocalInputHubRequestedMask() != FIXED_THREE_HUMAN_MASK ||
        SudekiMpLocalInputHubConnectedMask() != FIXED_THREE_HUMAN_MASK ||
        !readable_memory(
            game_base + RVA_ACTIVE_GROUP_GLOBAL, sizeof(group)) ||
        !readable_memory(
            game_base + RVA_CHARACTER_CONTROLLER_GLOBAL,
            sizeof(controller))) {
        return FALSE;
    }
    group = *(uint8_t **)(game_base + RVA_ACTIVE_GROUP_GLOBAL);
    controller = *(uint8_t **)(game_base + RVA_CHARACTER_CONTROLLER_GLOBAL);
    if (group == NULL || controller == NULL ||
        !readable_memory(group + PARTY_SLOT_ZERO_OFFSET,
            PARTY_SLOT_COUNT * PARTY_SLOT_STRIDE) ||
        !readable_memory(controller + CONTROLLER_TARGET_OFFSET,
            sizeof(controller_target))) {
        return FALSE;
    }
    controller_target = *(void **)(controller + CONTROLLER_TARGET_OFFSET);
    ZeroMemory(candidate, sizeof(*candidate));
    candidate->group = group;
    candidate->host_controller = controller;
    for (seat_index = 0u; seat_index < FIXED_THREE_SEAT_COUNT;
            ++seat_index) {
        const SudekiMpPlayerLease *lease = &statehood->players[seat_index];
        unsigned int occurrences = 0u;

        if (!lease->human_present || lease->actor == 0u ||
            lease->actor_generation == 0u ||
            !character_has_resource_type(
                (void *)lease->actor,
                assignment.actor_type_by_seat[seat_index])) {
            return FALSE;
        }
        candidate->actors[seat_index] = (void *)lease->actor;
        candidate->actor_generations[seat_index] = lease->actor_generation;
        candidate->party_slots[seat_index] = PARTY_SLOT_COUNT;
        for (party_index = 0u; party_index < PARTY_SLOT_COUNT;
                ++party_index) {
            void *party_actor = *(void **)(group + PARTY_SLOT_ZERO_OFFSET +
                party_index * PARTY_SLOT_STRIDE);

            if (party_actor == candidate->actors[seat_index]) {
                candidate->party_slots[seat_index] = party_index;
                ++occurrences;
            }
        }
        if (occurrences != 1u) {
            return FALSE;
        }
        if (seat_index != 0u &&
            (candidate->actors[seat_index] == candidate->actors[0] ||
             (seat_index == 2u && candidate->actors[seat_index] ==
                candidate->actors[1]))) {
            return FALSE;
        }
    }
    if (candidate->party_slots[0] != 0u ||
        controller_target != candidate->actors[0]) {
        return FALSE;
    }
    candidate->input_identities[0] = controller;
    candidate->input_generations[0] = 0u;
    {
        SudekiMpPlayerCombatSnapshot host_snapshot;

        if (!SudekiMpCombatContextGetSnapshot(0u, &host_snapshot) ||
            host_snapshot.character != candidate->actors[0] ||
            host_snapshot.input_source_kind !=
                SUDEKIMP_COMBAT_INPUT_NATIVE_CONTROLLER ||
            host_snapshot.input_source != controller) {
            return FALSE;
        }
    }
    for (seat_index = FIXED_THREE_COMPANION_FIRST_SEAT;
            seat_index <= FIXED_THREE_COMPANION_LAST_SEAT;
            ++seat_index) {
        SudekiMpPlayerCombatSnapshot snapshot;
        const void *identity =
            SudekiMpLocalInputHubSeatIdentity(seat_index);
        uint32_t generation =
            SudekiMpLocalInputHubSeatIdentityGeneration(seat_index);

        if (!SudekiMpControlSeparationSeatRequested(seat_index) ||
            !SudekiMpControlSeparationSeatActive(seat_index) ||
            !SudekiMpControlSeparationSeatInputReady(seat_index) ||
            !SudekiMpControlSeparationSeatInputLeaseActive(seat_index) ||
            SudekiMpControlSeparationSeatCharacter(seat_index) !=
                candidate->actors[seat_index] ||
            identity == NULL || generation == 0u ||
            !SudekiMpCombatContextGetSnapshot(seat_index, &snapshot) ||
            snapshot.character != candidate->actors[seat_index] ||
            snapshot.input_source_kind !=
                SUDEKIMP_COMBAT_INPUT_EXTERNAL_BRIDGE ||
            snapshot.input_source != identity) {
            return FALSE;
        }
        candidate->input_identities[seat_index] = identity;
        candidate->input_generations[seat_index] = generation;
    }
    return TRUE;
}

static void fixed_three_clear_published_view_if_exact(
    unsigned int seat_index,
    void *camera,
    void *render_state
) {
    SudekiMpPlayerCombatSnapshot snapshot;

    if (seat_index < FIXED_THREE_SEAT_COUNT && camera != NULL &&
        render_state != NULL &&
        SudekiMpCombatContextGetSnapshot(seat_index, &snapshot) &&
        snapshot.viewport_camera == camera &&
        snapshot.render_state == render_state) {
        SudekiMpCombatContextSetView(seat_index, NULL, NULL);
    }
}

static void fixed_three_clear_camera_runtime(void) {
    unsigned int seat_index;

    for (seat_index = 0u; seat_index < FIXED_THREE_SEAT_COUNT;
            ++seat_index) {
        fixed_three_clear_published_view_if_exact(
            seat_index,
            fixed_three_runtime.cameras[seat_index],
            fixed_three_runtime.render_states[seat_index]);
    }
    fixed_three_runtime.manager = NULL;
    fixed_three_runtime.group = NULL;
    fixed_three_runtime.host_controller = NULL;
    ZeroMemory(fixed_three_runtime.actors,
        sizeof(fixed_three_runtime.actors));
    ZeroMemory(fixed_three_runtime.actor_generations,
        sizeof(fixed_three_runtime.actor_generations));
    ZeroMemory(fixed_three_runtime.party_slots,
        sizeof(fixed_three_runtime.party_slots));
    ZeroMemory(fixed_three_runtime.input_identities,
        sizeof(fixed_three_runtime.input_identities));
    ZeroMemory(fixed_three_runtime.input_generations,
        sizeof(fixed_three_runtime.input_generations));
    ZeroMemory(fixed_three_runtime.cameras,
        sizeof(fixed_three_runtime.cameras));
    ZeroMemory(fixed_three_runtime.render_states,
        sizeof(fixed_three_runtime.render_states));
    ZeroMemory(fixed_three_runtime.camera_transform_initialized,
        sizeof(fixed_three_runtime.camera_transform_initialized));
    ZeroMemory(fixed_three_runtime.camera_last_target,
        sizeof(fixed_three_runtime.camera_last_target));
    ZeroMemory(fixed_three_runtime.camera_pitch_offset,
        sizeof(fixed_three_runtime.camera_pitch_offset));
    ZeroMemory(fixed_three_runtime.camera_input_last_tick,
        sizeof(fixed_three_runtime.camera_input_last_tick));
    ZeroMemory(fixed_three_runtime.camera_input_logged,
        sizeof(fixed_three_runtime.camera_input_logged));
    fixed_three_runtime.cameras_acquired = FALSE;
    fixed_three_runtime.scheduled_seat = 0u;
    fixed_three_runtime.rendered_seat = 0u;
    fixed_three_runtime.rejection_logged = FALSE;
    fixed_three_invalidate_frame_cache();
}

static BOOL fixed_three_restore_render_swap(void) {
    if (!fixed_three_runtime.render_swap_active) {
        return TRUE;
    }
    if (!writable_memory(fixed_three_runtime.render_swap_slot,
            sizeof(*fixed_three_runtime.render_swap_slot)) ||
        *fixed_three_runtime.render_swap_slot !=
            fixed_three_runtime.render_swap_applied_state) {
        return FALSE;
    }
    *fixed_three_runtime.render_swap_slot =
        fixed_three_runtime.render_swap_native_state;
    fixed_three_runtime.render_swap_slot = NULL;
    fixed_three_runtime.render_swap_native_state = NULL;
    fixed_three_runtime.render_swap_applied_state = NULL;
    fixed_three_runtime.render_swap_active = FALSE;
    return TRUE;
}

static BOOL fixed_three_release_cameras(const char *reason) {
    void *manager = fixed_three_runtime.manager;
    void *live_manager;
    void *global_camera;
    void *global_state;
    void **scene_slot;
    int seat_index;

    fixed_three_invalidate_frame_cache();
    if (!fixed_three_restore_render_swap()) {
        return FALSE;
    }
    if (!fixed_three_runtime.cameras_acquired) {
        fixed_three_clear_camera_runtime();
        fixed_three_release_frame_surfaces();
        return TRUE;
    }
    if (game_base == NULL || manager == NULL ||
        camera_manager_get_camera == NULL ||
        camera_manager_remove_camera == NULL ||
        !readable_memory(
            game_base + RVA_CAMERA_MANAGER_GLOBAL,
            sizeof(live_manager))) {
        return FALSE;
    }
    live_manager = *(void **)(game_base + RVA_CAMERA_MANAGER_GLOBAL);
    global_camera = current_render_camera(manager);
    global_state = readable_memory(global_camera, 0x38u) ?
        *(void **)((uint8_t *)global_camera + CAMERA_RENDER_STATE_OFFSET) :
        NULL;
    scene_slot = current_scene_render_camera_slot();
    if (live_manager != manager ||
        global_camera != fixed_three_runtime.cameras[0] ||
        global_state != fixed_three_runtime.render_states[0] ||
        scene_slot == NULL || !readable_memory(scene_slot, sizeof(*scene_slot)) ||
        *scene_slot != fixed_three_runtime.render_states[0]) {
        return FALSE;
    }
    for (seat_index = (int)FIXED_THREE_COMPANION_LAST_SEAT;
            seat_index >= (int)FIXED_THREE_COMPANION_FIRST_SEAT;
            --seat_index) {
        const char *name = seat_index == 2 ?
            third_player_camera_name : second_player_camera_name;
        void *saved_camera = fixed_three_runtime.cameras[seat_index];
        void *saved_render_state =
            fixed_three_runtime.render_states[seat_index];
        void *named_camera;

        if (saved_camera == NULL) {
            continue;
        }
        named_camera = camera_manager_get_camera(manager, name);
        if (named_camera != saved_camera) {
            return FALSE;
        }
        camera_manager_remove_camera(manager, name);
        if (camera_manager_get_camera(manager, name) == saved_camera) {
            return FALSE;
        }
        /* A later P2 verification/removal can still fail after P3 was
         * successfully removed.  Retire this seat's exact published view
         * before discarding the saved identity so a retry cannot strand a
         * CombatContext snapshot on the removed named camera. */
        fixed_three_clear_published_view_if_exact(
            (unsigned int)seat_index,
            saved_camera,
            saved_render_state);
        fixed_three_runtime.cameras[seat_index] = NULL;
        fixed_three_runtime.render_states[seat_index] = NULL;
    }
    SudekiMpLogFormat(
        "split_screen_render event=fixed_three_seat_camera phase=release "
        "reason=%s order=P3,P2 policy=exact_named_identity_reverse_teardown\r\n",
        reason == NULL ? "unspecified" : reason);
    fixed_three_clear_camera_runtime();
    fixed_three_release_frame_surfaces();
    return TRUE;
}

static BOOL fixed_three_acquire_cameras(void) {
    SudekiMpFixedThreeSeatLeaseCandidate candidate;
    void *manager;
    void *host_camera;
    void *host_render_state;
    void **scene_slot;
    unsigned int seat_index;

    if (!fixed_three_collect_leases(&candidate)) {
        return FALSE;
    }
    if (player_two_camera != NULL &&
        !release_player_two_camera("fixed_three_seat_enter")) {
        return FALSE;
    }
    manager = current_camera_manager();
    host_camera = current_render_camera(manager);
    host_render_state = readable_memory(host_camera, 0x38u) ?
        *(void **)((uint8_t *)host_camera + CAMERA_RENDER_STATE_OFFSET) :
        NULL;
    scene_slot = current_scene_render_camera_slot();
    if (manager == NULL || !readable_memory(host_camera, 0x108u) ||
        !render_state_camera_matrix_valid(host_render_state) ||
        scene_slot == NULL || !readable_memory(scene_slot, sizeof(*scene_slot)) ||
        *scene_slot != host_render_state ||
        camera_manager_get_camera(manager, second_player_camera_name) != NULL ||
        camera_manager_get_camera(manager, third_player_camera_name) != NULL) {
        return FALSE;
    }
    fixed_three_runtime.manager = manager;
    fixed_three_runtime.cameras[0] = host_camera;
    fixed_three_runtime.render_states[0] = host_render_state;
    for (seat_index = FIXED_THREE_COMPANION_FIRST_SEAT;
            seat_index <= FIXED_THREE_COMPANION_LAST_SEAT;
            ++seat_index) {
        const char *name = seat_index == 2u ?
            third_player_camera_name : second_player_camera_name;
        void *camera;
        void *render_state;

        if (!camera_manager_add_camera(manager, name, "default")) {
            fixed_three_runtime.cameras_acquired = TRUE;
            (void)fixed_three_release_cameras("acquire_add_failed");
            return FALSE;
        }
        camera = camera_manager_get_camera(manager, name);
        render_state = readable_memory(camera, 0x108u) ?
            *(void **)((uint8_t *)camera + CAMERA_RENDER_STATE_OFFSET) :
            NULL;
        if (!readable_memory(camera, 0x108u) ||
            !readable_memory(render_state, 0xdcu)) {
            fixed_three_runtime.cameras[seat_index] = camera;
            fixed_three_runtime.render_states[seat_index] = render_state;
            fixed_three_runtime.cameras_acquired = TRUE;
            (void)fixed_three_release_cameras("acquire_identity_failed");
            return FALSE;
        }
        fixed_three_runtime.cameras[seat_index] = camera;
        fixed_three_runtime.render_states[seat_index] = render_state;
    }
    fixed_three_runtime.group = candidate.group;
    fixed_three_runtime.host_controller = candidate.host_controller;
    memcpy(fixed_three_runtime.actors, candidate.actors,
        sizeof(candidate.actors));
    memcpy(fixed_three_runtime.actor_generations,
        candidate.actor_generations, sizeof(candidate.actor_generations));
    memcpy(fixed_three_runtime.party_slots, candidate.party_slots,
        sizeof(candidate.party_slots));
    memcpy(fixed_three_runtime.input_identities,
        candidate.input_identities, sizeof(candidate.input_identities));
    memcpy(fixed_three_runtime.input_generations,
        candidate.input_generations, sizeof(candidate.input_generations));
    fixed_three_runtime.cameras_acquired = TRUE;
    fixed_three_runtime.scheduled_seat = 0u;
    fixed_three_runtime.rendered_seat = 0u;
    for (seat_index = 0u; seat_index < FIXED_THREE_SEAT_COUNT;
            ++seat_index) {
        SudekiMpCombatContextSetView(
            seat_index,
            fixed_three_runtime.cameras[seat_index],
            fixed_three_runtime.render_states[seat_index]);
    }
    fixed_three_invalidate_frame_cache();
    SudekiMpLogFormat(
        "split_screen_render event=fixed_three_seat_camera phase=acquire "
        "manager=0x%08lx p1=0x%08lx p2=0x%08lx p3=0x%08lx "
        "actors=0x%08lx,0x%08lx,0x%08lx "
        "policy=named_manual_cameras_no_native_collision_or_special_camera_routing\r\n",
        (unsigned long)(uintptr_t)manager,
        (unsigned long)(uintptr_t)fixed_three_runtime.cameras[0],
        (unsigned long)(uintptr_t)fixed_three_runtime.cameras[1],
        (unsigned long)(uintptr_t)fixed_three_runtime.cameras[2],
        (unsigned long)(uintptr_t)fixed_three_runtime.actors[0],
        (unsigned long)(uintptr_t)fixed_three_runtime.actors[1],
        (unsigned long)(uintptr_t)fixed_three_runtime.actors[2]);
    return TRUE;
}

static BOOL fixed_three_seat_view_exact(
    unsigned int seat_index,
    const void *character
) {
    SudekiMpFixedThreeSeatLeaseCandidate candidate;
    SudekiMpPlayerCombatSnapshot snapshot;
    void *live_camera;
    void *live_render_state;
    void **scene_slot;
    const char *name;

    if (seat_index >= FIXED_THREE_SEAT_COUNT ||
        !fixed_three_runtime.configured ||
        !fixed_three_assignment_selected() ||
        !fixed_three_runtime.cameras_acquired || character == NULL ||
        character != fixed_three_runtime.actors[seat_index] ||
        !fixed_three_collect_leases(&candidate) ||
        candidate.group != fixed_three_runtime.group ||
        candidate.host_controller != fixed_three_runtime.host_controller ||
        candidate.actors[seat_index] != character ||
        candidate.actor_generations[seat_index] !=
            fixed_three_runtime.actor_generations[seat_index] ||
        candidate.party_slots[seat_index] !=
            fixed_three_runtime.party_slots[seat_index] ||
        candidate.input_identities[seat_index] !=
            fixed_three_runtime.input_identities[seat_index] ||
        candidate.input_generations[seat_index] !=
            fixed_three_runtime.input_generations[seat_index] ||
        fixed_three_runtime.cameras[seat_index] == NULL ||
        !render_state_camera_matrix_valid(
            fixed_three_runtime.render_states[seat_index]) ||
        !SudekiMpCombatContextGetSnapshot(seat_index, &snapshot) ||
        snapshot.character != character ||
        (seat_index == 0u ?
            (snapshot.input_source_kind !=
                SUDEKIMP_COMBAT_INPUT_NATIVE_CONTROLLER ||
             snapshot.input_source != candidate.host_controller) :
            (snapshot.input_source_kind !=
                SUDEKIMP_COMBAT_INPUT_EXTERNAL_BRIDGE ||
             snapshot.input_source !=
                candidate.input_identities[seat_index])) ||
        snapshot.viewport_camera != fixed_three_runtime.cameras[seat_index] ||
        snapshot.render_state != fixed_three_runtime.render_states[seat_index]) {
        return FALSE;
    }
    live_camera = current_render_camera(fixed_three_runtime.manager);
    live_render_state = readable_memory(live_camera, 0x38u) ?
        *(void **)((uint8_t *)live_camera + CAMERA_RENDER_STATE_OFFSET) :
        NULL;
    scene_slot = current_scene_render_camera_slot();
    if (current_camera_manager() != fixed_three_runtime.manager ||
        live_camera != fixed_three_runtime.cameras[0] ||
        live_render_state != fixed_three_runtime.render_states[0] ||
        scene_slot == NULL || !readable_memory(scene_slot, sizeof(*scene_slot)) ||
        *scene_slot != fixed_three_runtime.render_states[0]) {
        return FALSE;
    }
    if (seat_index == 0u) {
        return TRUE;
    }
    name = seat_index == 2u ?
        third_player_camera_name : second_player_camera_name;
    return current_camera_manager() == fixed_three_runtime.manager &&
        camera_manager_get_camera(fixed_three_runtime.manager, name) ==
            fixed_three_runtime.cameras[seat_index];
}

static BOOL fixed_three_layout_exact(void) {
    unsigned int width = fixed_three_runtime.frame_description.width;
    unsigned int height = fixed_three_runtime.frame_description.height;
    unsigned int half_width = width / 2u;
    unsigned int half_height = height / 2u;
    const SudekiMpLocalViewportLayout *layout = &fixed_three_runtime.layout;

    return fixed_three_runtime.frame_device != NULL &&
        width >= 2u && height >= 2u &&
        layout->active_human_mask == FIXED_THREE_HUMAN_MASK &&
        layout->viewport_count == FIXED_THREE_SEAT_COUNT &&
        layout->surface_width == width &&
        layout->surface_height == height &&
        layout->viewports[0].seat_index == 0u &&
        layout->viewports[0].rectangle.x == 0u &&
        layout->viewports[0].rectangle.y == 0u &&
        layout->viewports[0].rectangle.width == width &&
        layout->viewports[0].rectangle.height == half_height &&
        layout->viewports[1].seat_index == 1u &&
        layout->viewports[1].rectangle.x == 0u &&
        layout->viewports[1].rectangle.y == half_height &&
        layout->viewports[1].rectangle.width == half_width &&
        layout->viewports[1].rectangle.height == height - half_height &&
        layout->viewports[2].seat_index == 2u &&
        layout->viewports[2].rectangle.x == half_width &&
        layout->viewports[2].rectangle.y == half_height &&
        layout->viewports[2].rectangle.width == width - half_width &&
        layout->viewports[2].rectangle.height == height - half_height;
}

BOOL SudekiMpSplitScreenSeatViewReady(
    unsigned int seat_index,
    const void *character
) {
    SudekiMpPlayerCombatSnapshot snapshot;

    if (fixed_three_assignment_selected()) {
        /* Camera publication precedes cache warmup.  Gameplay submission is
         * not admitted until every seat has a fresh full-frame cache and the
         * exact P1-top/P2-bottom-left/P3-bottom-right layout is the currently
         * composed ownership contract. */
        return fixed_three_runtime.frame_valid_mask ==
                FIXED_THREE_HUMAN_MASK &&
            fixed_three_runtime.frame_owner_evidence_mask ==
                FIXED_THREE_HUMAN_MASK &&
            fixed_three_layout_exact() &&
            fixed_three_seat_view_exact(seat_index, character);
    }
    if (seat_index != 1u || character == NULL ||
        character != player_two_character || player_two_camera == NULL ||
        !player_two_camera_transform_initialized ||
        !render_state_camera_matrix_valid(player_two_render_state) ||
        !SudekiMpCombatContextGetSnapshot(1u, &snapshot)) {
        return FALSE;
    }
    return snapshot.character == character &&
        snapshot.viewport_camera == player_two_camera &&
        snapshot.render_state == player_two_render_state;
}

static BOOL fixed_three_base_leases_exact(void) {
    SudekiMpFixedThreeSeatLeaseCandidate candidate;
    void *live_camera;
    void *live_render_state;
    void **scene_slot;
    unsigned int seat_index;

    live_camera = current_render_camera(fixed_three_runtime.manager);
    live_render_state = readable_memory(live_camera, 0x38u) ?
        *(void **)((uint8_t *)live_camera + CAMERA_RENDER_STATE_OFFSET) :
        NULL;
    scene_slot = current_scene_render_camera_slot();
    if (!fixed_three_runtime.cameras_acquired ||
        fixed_three_runtime.render_swap_active ||
        !fixed_three_collect_leases(&candidate) ||
        candidate.group != fixed_three_runtime.group ||
        candidate.host_controller != fixed_three_runtime.host_controller ||
        current_camera_manager() != fixed_three_runtime.manager ||
        live_camera != fixed_three_runtime.cameras[0] ||
        live_render_state != fixed_three_runtime.render_states[0] ||
        scene_slot == NULL || !readable_memory(scene_slot, sizeof(*scene_slot)) ||
        *scene_slot != fixed_three_runtime.render_states[0]) {
        return FALSE;
    }
    for (seat_index = 0u; seat_index < FIXED_THREE_SEAT_COUNT;
            ++seat_index) {
        const char *name;

        if (candidate.actors[seat_index] !=
                fixed_three_runtime.actors[seat_index] ||
            candidate.actor_generations[seat_index] !=
                fixed_three_runtime.actor_generations[seat_index] ||
            candidate.party_slots[seat_index] !=
                fixed_three_runtime.party_slots[seat_index] ||
            candidate.input_identities[seat_index] !=
                fixed_three_runtime.input_identities[seat_index] ||
            candidate.input_generations[seat_index] !=
                fixed_three_runtime.input_generations[seat_index] ||
            !render_state_camera_matrix_valid(
                fixed_three_runtime.render_states[seat_index])) {
            return FALSE;
        }
        if (seat_index == 0u) {
            continue;
        }
        name = seat_index == 2u ?
            third_player_camera_name : second_player_camera_name;
        if (camera_manager_get_camera(
                fixed_three_runtime.manager, name) !=
            fixed_three_runtime.cameras[seat_index]) {
            return FALSE;
        }
    }
    return TRUE;
}

static BOOL fixed_three_apply_camera_input(
    unsigned int seat_index,
    float matrix[16],
    const float target[3]
) {
    SudekiMpInputBridgeState input;
    DWORD now = GetTickCount();
    DWORD elapsed_ms;
    float raw_x;
    float raw_y;
    float raw_magnitude;
    float magnitude;
    float scaled_magnitude;
    float axis_x;
    float axis_y;
    float yaw_delta;
    float pitch_delta;
    float proposed_pitch;

    if (seat_index < FIXED_THREE_COMPANION_FIRST_SEAT ||
        seat_index > FIXED_THREE_COMPANION_LAST_SEAT) {
        return FALSE;
    }
    if (SudekiMpControlSeparationGameplayInputFrozen()) {
        fixed_three_runtime.camera_input_last_tick[seat_index] = now;
        return TRUE;
    }
    elapsed_ms = fixed_three_runtime.camera_input_last_tick[seat_index] == 0u ?
        0u : (DWORD)(now -
            fixed_three_runtime.camera_input_last_tick[seat_index]);
    fixed_three_runtime.camera_input_last_tick[seat_index] = now;
    if (elapsed_ms > 100u) {
        elapsed_ms = 100u;
    }
    if (!SudekiMpLocalInputHubPoll(seat_index, &input) ||
        elapsed_ms == 0u ||
        SudekiMpLocalInputHubSeatIdentity(seat_index) !=
            fixed_three_runtime.input_identities[seat_index] ||
        SudekiMpLocalInputHubSeatIdentityGeneration(seat_index) !=
            fixed_three_runtime.input_generations[seat_index]) {
        return elapsed_ms == 0u;
    }
    raw_x = (float)input.right_x / 32768.0f;
    raw_y = -(float)input.right_y / 32768.0f;
    raw_magnitude = sqrtf(raw_x * raw_x + raw_y * raw_y);
    if (!isfinite(raw_magnitude)) {
        return FALSE;
    }
    if (raw_magnitude <= second_player_controller_camera_deadzone) {
        return TRUE;
    }
    magnitude = raw_magnitude > 1.0f ? 1.0f : raw_magnitude;
    scaled_magnitude =
        (magnitude - second_player_controller_camera_deadzone) /
        (1.0f - second_player_controller_camera_deadzone);
    axis_x = raw_x / raw_magnitude * scaled_magnitude;
    axis_y = raw_y / raw_magnitude * scaled_magnitude;
    yaw_delta = -axis_x * second_player_controller_camera_yaw_speed *
        ((float)elapsed_ms / 1000.0f);
    pitch_delta = axis_y * second_player_controller_camera_pitch_speed *
        ((float)elapsed_ms / 1000.0f);
    proposed_pitch =
        fixed_three_runtime.camera_pitch_offset[seat_index] + pitch_delta;
    if (proposed_pitch > second_player_controller_camera_maximum_pitch) {
        proposed_pitch = second_player_controller_camera_maximum_pitch;
    } else if (proposed_pitch <
            -second_player_controller_camera_maximum_pitch) {
        proposed_pitch = -second_player_controller_camera_maximum_pitch;
    }
    pitch_delta = proposed_pitch -
        fixed_three_runtime.camera_pitch_offset[seat_index];
    if (!SudekiMpOrbitCameraTransform(
            matrix, target, yaw_delta, pitch_delta)) {
        return FALSE;
    }
    fixed_three_runtime.camera_pitch_offset[seat_index] = proposed_pitch;
    if (!fixed_three_runtime.camera_input_logged[seat_index]) {
        fixed_three_runtime.camera_input_logged[seat_index] = TRUE;
        SudekiMpLogFormat(
            "split_screen_render event=fixed_three_seat_orbit player=%u "
            "phase=active input_generation=%lu "
            "policy=independent_right_stick_manual_named_camera\r\n",
            seat_index + 1u,
            (unsigned long)
                fixed_three_runtime.input_generations[seat_index]);
    }
    return TRUE;
}

static BOOL fixed_three_update_manual_views(BOOL allow_user_orbit) {
    float host_position[3];
    unsigned int seat_index;

    if (!fixed_three_base_leases_exact() ||
        !character_position(fixed_three_runtime.actors[0], host_position)) {
        return FALSE;
    }
    SudekiMpCombatContextSetView(
        0u,
        fixed_three_runtime.cameras[0],
        fixed_three_runtime.render_states[0]);
    for (seat_index = FIXED_THREE_COMPANION_FIRST_SEAT;
            seat_index <= FIXED_THREE_COMPANION_LAST_SEAT;
            ++seat_index) {
        float actor_position[3];
        float target[3];
        float matrix[16];
        float target_delta[3];
        uint16_t *generation;

        if (!character_position(
                fixed_three_runtime.actors[seat_index], actor_position)) {
            return FALSE;
        }
        memcpy(target, actor_position, sizeof(target));
        target[1] += 1.0f;
        if (!fixed_three_runtime.camera_transform_initialized[seat_index]) {
            memcpy(matrix,
                (uint8_t *)fixed_three_runtime.render_states[0] + 0x90u,
                sizeof(matrix));
            matrix[12] += actor_position[0] - host_position[0];
            matrix[13] += actor_position[1] - host_position[1];
            matrix[14] += actor_position[2] - host_position[2];
            memcpy(fixed_three_runtime.camera_last_target[seat_index],
                target, sizeof(target));
            fixed_three_runtime.camera_input_last_tick[seat_index] =
                GetTickCount();
        } else {
            memcpy(matrix,
                (uint8_t *)fixed_three_runtime.render_states[seat_index] +
                    0x90u,
                sizeof(matrix));
            target_delta[0] = target[0] -
                fixed_three_runtime.camera_last_target[seat_index][0];
            target_delta[1] = target[1] -
                fixed_three_runtime.camera_last_target[seat_index][1];
            target_delta[2] = target[2] -
                fixed_three_runtime.camera_last_target[seat_index][2];
            matrix[12] += target_delta[0];
            matrix[13] += target_delta[1];
            matrix[14] += target_delta[2];
            memcpy(fixed_three_runtime.camera_last_target[seat_index],
                target, sizeof(target));
        }
        if (allow_user_orbit) {
            if (!fixed_three_apply_camera_input(
                    seat_index, matrix, target)) {
                return FALSE;
            }
        } else {
            /* Warmup and invalidation may prepare/follow manual cameras, but
             * right-stick time cannot accumulate behind a full-width or
             * otherwise unproven presentation. */
            fixed_three_runtime.camera_input_last_tick[seat_index] =
                GetTickCount();
        }
        matrix[15] = 1.0f;
        if (!SudekiMpSplitScreenNativeCameraMatrixPolicy(matrix)) {
            return FALSE;
        }
        memcpy((uint8_t *)fixed_three_runtime.render_states[seat_index] +
                0x90u,
            matrix, sizeof(matrix));
        memcpy((uint8_t *)fixed_three_runtime.render_states[seat_index] +
                0xd0u,
            (uint8_t *)fixed_three_runtime.render_states[0] + 0xd0u,
            sizeof(float) * 3u);
        generation = (uint16_t *)(
            (uint8_t *)fixed_three_runtime.render_states[seat_index] +
            RENDER_STATE_GENERATION_OFFSET);
        ++*generation;
        fixed_three_runtime.camera_transform_initialized[seat_index] = TRUE;
        SudekiMpCombatContextSetView(
            seat_index,
            fixed_three_runtime.cameras[seat_index],
            fixed_three_runtime.render_states[seat_index]);
    }
    return fixed_three_seat_view_exact(
            0u, fixed_three_runtime.actors[0]) &&
        fixed_three_seat_view_exact(
            1u, fixed_three_runtime.actors[1]) &&
        fixed_three_seat_view_exact(
            2u, fixed_three_runtime.actors[2]);
}

static BOOL fixed_three_apply_render_seat(
    unsigned int seat_index,
    BOOL allow_user_orbit
) {
    void *native_camera;
    void *native_state;
    void **scene_slot;

    if (seat_index >= FIXED_THREE_SEAT_COUNT ||
        !fixed_three_update_manual_views(allow_user_orbit)) {
        return FALSE;
    }
    native_camera = current_render_camera(fixed_three_runtime.manager);
    native_state = readable_memory(native_camera, 0x38u) ?
        *(void **)((uint8_t *)native_camera + CAMERA_RENDER_STATE_OFFSET) :
        NULL;
    scene_slot = current_scene_render_camera_slot();
    if (native_camera != fixed_three_runtime.cameras[0] ||
        native_state != fixed_three_runtime.render_states[0] ||
        scene_slot == NULL || !writable_memory(scene_slot, sizeof(*scene_slot)) ||
        *scene_slot != native_state) {
        return FALSE;
    }
    if (seat_index == 0u) {
        return TRUE;
    }
    *scene_slot = fixed_three_runtime.render_states[seat_index];
    fixed_three_runtime.render_swap_slot = scene_slot;
    fixed_three_runtime.render_swap_native_state = native_state;
    fixed_three_runtime.render_swap_applied_state =
        fixed_three_runtime.render_states[seat_index];
    fixed_three_runtime.render_swap_active = TRUE;
    return TRUE;
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

static BOOL ensure_fixed_three_frame_surfaces(
    void *device,
    D3DCreateTextureFunction create_texture,
    const SudekiMpD3DSurfaceDesc *source_description
) {
    unsigned int seat_index;

    if (fixed_three_runtime.frame_device == device &&
        same_surface_description(
            &fixed_three_runtime.frame_description,
            source_description)) {
        for (seat_index = 0u; seat_index < FIXED_THREE_SEAT_COUNT;
                ++seat_index) {
            if (fixed_three_runtime.frame_surfaces[seat_index] == NULL ||
                fixed_three_runtime.frame_textures[seat_index] == NULL) {
                break;
            }
        }
        if (seat_index == FIXED_THREE_SEAT_COUNT) {
            return TRUE;
        }
    }
    fixed_three_release_frame_surfaces();
    for (seat_index = 0u; seat_index < FIXED_THREE_SEAT_COUNT;
            ++seat_index) {
        void **texture_vtable;
        D3DTextureGetSurfaceLevelFunction get_surface_level;
        HRESULT result = create_texture(
            device,
            source_description->width,
            source_description->height,
            1u,
            D3D_USAGE_RENDER_TARGET,
            source_description->format,
            D3D_POOL_DEFAULT,
            &fixed_three_runtime.frame_textures[seat_index],
            NULL);

        if (FAILED(result) ||
            fixed_three_runtime.frame_textures[seat_index] == NULL) {
            log_failure_once("create_fixed_three_frame_texture_failed",
                result);
            fixed_three_release_frame_surfaces();
            return FALSE;
        }
        texture_vtable = *(void ***)
            fixed_three_runtime.frame_textures[seat_index];
        if (!readable_memory(texture_vtable,
                (D3D_TEXTURE_GET_SURFACE_LEVEL_INDEX + 1u) *
                    sizeof(void *))) {
            log_failure_once("fixed_three_texture_vtable_unavailable",
                E_POINTER);
            fixed_three_release_frame_surfaces();
            return FALSE;
        }
        get_surface_level = (D3DTextureGetSurfaceLevelFunction)
            texture_vtable[D3D_TEXTURE_GET_SURFACE_LEVEL_INDEX];
        result = get_surface_level(
            fixed_three_runtime.frame_textures[seat_index],
            0u,
            &fixed_three_runtime.frame_surfaces[seat_index]);
        if (FAILED(result) ||
            fixed_three_runtime.frame_surfaces[seat_index] == NULL) {
            log_failure_once("get_fixed_three_frame_surface_failed",
                result);
            fixed_three_release_frame_surfaces();
            return FALSE;
        }
    }
    fixed_three_runtime.frame_device = device;
    fixed_three_runtime.frame_description = *source_description;
    fixed_three_runtime.frame_description.multisample_type =
        D3D_MULTISAMPLE_NONE;
    fixed_three_runtime.frame_description.multisample_quality = 0u;
    fixed_three_invalidate_frame_cache();
    return TRUE;
}

static BOOL fixed_three_build_layout(
    const SudekiMpD3DSurfaceDesc *description
) {
    uint8_t controller_slots[SUDEKIMP_LOCAL_VIEWPORT_MAX_SEATS];
    unsigned int seat_index;

    for (seat_index = 0u; seat_index < SUDEKIMP_LOCAL_VIEWPORT_MAX_SEATS;
            ++seat_index) {
        controller_slots[seat_index] =
            SUDEKIMP_LOCAL_VIEWPORT_NO_CONTROLLER;
    }
    for (seat_index = FIXED_THREE_COMPANION_FIRST_SEAT;
            seat_index <= FIXED_THREE_COMPANION_LAST_SEAT;
            ++seat_index) {
        unsigned int controller =
            SudekiMpLocalInputHubSeatController(seat_index);

        /* UDP has no XInput index.  Its stable transport lanes are P2=0 and
         * P3=1, matching base_port/base_port+1 and remaining distinct from
         * seat ownership. */
        controller_slots[seat_index] = (uint8_t)(controller < 4u ?
            controller : seat_index - 1u);
    }
    return SudekiMpLocalViewportLayoutBuild(
        FIXED_THREE_HUMAN_MASK,
        controller_slots,
        description->width,
        description->height,
        &fixed_three_runtime.layout) ? TRUE : FALSE;
}

static BOOL compose_fixed_three_camera_frames(
    unsigned int rendered_seat,
    BOOL owner_evidence_ready
) {
    void *device;
    void **device_vtable;
    D3DCreateTextureFunction create_texture;
    D3DStretchRectFunction stretch_rect;
    D3DGetRenderTargetFunction get_render_target;
    void *native_surface = NULL;
    void **surface_vtable;
    D3DSurfaceGetDescFunction get_description;
    SudekiMpD3DSurfaceDesc description;
    RECT source_rectangle;
    HRESULT result;
    unsigned int seat_index;
    unsigned int actor_mask = 0u;
    unsigned int camera_mask = 0u;
    unsigned int render_state_mask = 0u;
    unsigned int hud_mask = 0u;
    unsigned int input_mask = 0u;
    BOOL layout_ready;
    BOOL activation_ready;

    if (rendered_seat >= FIXED_THREE_SEAT_COUNT || !owner_evidence_ready ||
        d3d_device_global == NULL ||
        !readable_memory(d3d_device_global, sizeof(*d3d_device_global))) {
        return FALSE;
    }
    device = *d3d_device_global;
    if (!readable_memory(device, sizeof(void *))) {
        return FALSE;
    }
    device_vtable = *(void ***)device;
    if (!readable_memory(device_vtable,
            (D3D_DEVICE_GET_RENDER_TARGET_INDEX + 1u) * sizeof(void *))) {
        return FALSE;
    }
    create_texture = (D3DCreateTextureFunction)
        device_vtable[D3D_DEVICE_CREATE_TEXTURE_INDEX];
    stretch_rect = (D3DStretchRectFunction)
        device_vtable[D3D_DEVICE_STRETCH_RECT_INDEX];
    get_render_target = (D3DGetRenderTargetFunction)
        device_vtable[D3D_DEVICE_GET_RENDER_TARGET_INDEX];
    if (create_texture == NULL || stretch_rect == NULL ||
        get_render_target == NULL) {
        return FALSE;
    }
    result = get_render_target(device, 0u, &native_surface);
    if (FAILED(result) || !readable_memory(native_surface, sizeof(void *))) {
        release_com_object(&native_surface);
        return FALSE;
    }
    surface_vtable = *(void ***)native_surface;
    if (!readable_memory(surface_vtable,
            (D3D_SURFACE_GET_DESC_INDEX + 1u) * sizeof(void *))) {
        release_com_object(&native_surface);
        return FALSE;
    }
    get_description = (D3DSurfaceGetDescFunction)
        surface_vtable[D3D_SURFACE_GET_DESC_INDEX];
    result = get_description(native_surface, &description);
    if (FAILED(result) || description.width < 2u ||
        description.height < 2u ||
        description.multisample_type != D3D_MULTISAMPLE_NONE ||
        !ensure_fixed_three_frame_surfaces(
            device, create_texture, &description)) {
        release_com_object(&native_surface);
        return FALSE;
    }
    source_rectangle.left = 0;
    source_rectangle.top = 0;
    source_rectangle.right = (LONG)description.width;
    source_rectangle.bottom = (LONG)description.height;
    result = stretch_rect(
        device,
        native_surface,
        &source_rectangle,
        fixed_three_runtime.frame_surfaces[rendered_seat],
        &source_rectangle,
        D3D_TEXTURE_FILTER_NONE);
    if (FAILED(result)) {
        fixed_three_invalidate_frame_cache();
        release_com_object(&native_surface);
        return FALSE;
    }
    fixed_three_runtime.frame_valid_mask |= 1u << rendered_seat;
    fixed_three_runtime.frame_owner_evidence_mask |= 1u << rendered_seat;
    layout_ready = fixed_three_build_layout(&description);
    for (seat_index = 0u; seat_index < FIXED_THREE_SEAT_COUNT;
            ++seat_index) {
        unsigned int bit = 1u << seat_index;

        if (fixed_three_seat_view_exact(
                seat_index, fixed_three_runtime.actors[seat_index])) {
            actor_mask |= bit;
            camera_mask |= bit;
            render_state_mask |= bit;
            input_mask |= bit;
        }
    }
    hud_mask = fixed_three_runtime.frame_owner_evidence_mask;
    activation_ready = fixed_three_runtime.frame_owner_evidence_mask ==
            FIXED_THREE_HUMAN_MASK &&
        SudekiMpSplitScreenAdaptiveSeatActivationPolicy(
            fixed_three_runtime.configured,
            FIXED_THREE_HUMAN_MASK,
            layout_ready,
            actor_mask,
            camera_mask,
            render_state_mask,
            hud_mask,
            input_mask,
            fixed_three_runtime.frame_valid_mask,
            fixed_three_runtime.presentation_clear_this_frame);
    if (!activation_ready) {
        release_com_object(&native_surface);
        return TRUE;
    }
    for (seat_index = 0u;
            seat_index < fixed_three_runtime.layout.viewport_count;
            ++seat_index) {
        const SudekiMpLocalSeatViewport *viewport =
            &fixed_three_runtime.layout.viewports[seat_index];
        RECT destination;

        destination.left = (LONG)viewport->rectangle.x;
        destination.top = (LONG)viewport->rectangle.y;
        destination.right = (LONG)(viewport->rectangle.x +
            viewport->rectangle.width);
        destination.bottom = (LONG)(viewport->rectangle.y +
            viewport->rectangle.height);
        result = stretch_rect(
            device,
            fixed_three_runtime.frame_surfaces[viewport->seat_index],
            &source_rectangle,
            native_surface,
            &destination,
            D3D_TEXTURE_FILTER_LINEAR);
        if (FAILED(result)) {
            (void)stretch_rect(
                device,
                fixed_three_runtime.frame_surfaces[rendered_seat],
                &source_rectangle,
                native_surface,
                &source_rectangle,
                D3D_TEXTURE_FILTER_NONE);
            fixed_three_invalidate_frame_cache();
            release_com_object(&native_surface);
            return FALSE;
        }
    }
    if (!fixed_three_runtime.compositor_logged) {
        fixed_three_runtime.compositor_logged = TRUE;
        SudekiMpLogFormat(
            "split_screen_render event=fixed_three_seat_cache phase=active "
            "source=%ux%u layout=p1_top_wide_p2_bottom_left_p3_bottom_right "
            "cadence=0,1,2 cache_ready_mask=0x%02lx "
            "policy=full_width_until_all_three_fresh\r\n",
            description.width,
            description.height,
            (unsigned long)fixed_three_runtime.frame_valid_mask);
    }
    release_com_object(&native_surface);
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

static BOOL compose_cached_camera_frames(
    BOOL rendered_player_two,
    BOOL capture_current_frame
) {
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
    BOOL capture_succeeded;

    if (d3d_device_global == NULL ||
        !readable_memory(d3d_device_global, sizeof(*d3d_device_global))) {
        log_failure_once("device_global_unavailable", E_POINTER);
        return FALSE;
    }
    device = *d3d_device_global;
    if (!readable_memory(device, sizeof(void *))) {
        log_failure_once("device_unavailable", E_POINTER);
        return FALSE;
    }
    device_vtable = *(void ***)device;
    if (!readable_memory(
            device_vtable,
            (D3D_DEVICE_GET_RENDER_TARGET_INDEX + 1u) * sizeof(void *))) {
        log_failure_once("device_vtable_unavailable", E_POINTER);
        return FALSE;
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
        return FALSE;
    }
    result = get_render_target(device, 0u, &native_surface);
    if (FAILED(result) || !readable_memory(native_surface, sizeof(void *))) {
        log_failure_once("get_native_surface_failed", result);
        release_com_object(&native_surface);
        return FALSE;
    }
    surface_vtable = *(void ***)native_surface;
    if (!readable_memory(
            surface_vtable,
            (D3D_SURFACE_GET_DESC_INDEX + 1u) * sizeof(void *))) {
        log_failure_once("surface_vtable_unavailable", E_POINTER);
        release_com_object(&native_surface);
        return FALSE;
    }
    get_description = (D3DSurfaceGetDescFunction)
        surface_vtable[D3D_SURFACE_GET_DESC_INDEX];
    result = get_description(native_surface, &description);
    if (FAILED(result) || description.width < 2u || description.height == 0u) {
        log_failure_once("get_surface_description_failed", result);
        release_com_object(&native_surface);
        return FALSE;
    }
    if (description.multisample_type != D3D_MULTISAMPLE_NONE) {
        log_failure_once("multisampled_backbuffer_unsupported", E_NOTIMPL);
        release_com_object(&native_surface);
        return FALSE;
    }
    if (!ensure_dual_frame_surfaces(
            device,
            create_texture,
            &description)) {
        release_com_object(&native_surface);
        return FALSE;
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
    capture_succeeded = TRUE;
    if (capture_current_frame) {
        result = stretch_rect(
            device,
            native_surface,
            &source_rectangle,
            current_frame_surface,
            &source_rectangle,
            D3D_TEXTURE_FILTER_NONE
        );
        capture_succeeded = SUCCEEDED(result);
        if (!capture_succeeded) {
            log_failure_once("capture_camera_frame_failed", result);
        } else {
            if (rendered_player_two) {
                player_two_frame_valid = TRUE;
            } else {
                player_one_frame_valid = TRUE;
            }
        }
    }
    if (!player_one_frame_valid || !player_two_frame_valid) {
        release_com_object(&native_surface);
        return capture_current_frame ? capture_succeeded : FALSE;
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
        return FALSE;
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
    return capture_succeeded;
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

static BOOL fixed_three_render_start_dispatch(BOOL runtime_authorized) {
    const char *gate_reason = "fixed_three_unavailable";
    BOOL gameplay_allowed = gameplay_split_allowed(
        runtime_authorized, &gate_reason);
    BOOL base_leases_exact;
    BOOL shared_modal_clear;
    BOOL presentation_clear;
    BOOL allow_user_orbit;

    fixed_three_reset_render_owner_evidence();
    fixed_three_runtime.transaction_active = TRUE;
    fixed_three_runtime.presentation_clear_this_frame = FALSE;
    fixed_three_runtime.rendered_seat = 0u;
    rendered_player_two_this_frame = FALSE;
    viewport_hud_binding_active = FALSE;
    genuine_quick_menu_active_this_frame = FALSE;
    quick_menu_live_player_two_available_this_frame = FALSE;
    if (!fixed_three_runtime.configured || !runtime_authorized ||
        !gameplay_allowed) {
        if (fixed_three_runtime.cameras_acquired) {
            (void)fixed_three_release_cameras(
                fixed_three_runtime.configured ?
                    "runtime_or_gameplay_gate_inactive" :
                    "feature_disabled");
        }
        original_render_start();
        (void)refresh_shared_interaction_modal();
        return TRUE;
    }
    if (!fixed_three_runtime.cameras_acquired) {
        if (!fixed_three_acquire_cameras() &&
            !fixed_three_runtime.rejection_logged) {
            fixed_three_runtime.rejection_logged = TRUE;
            SudekiMpLogWrite(
                "split_screen_render event=fixed_three_seat_camera "
                "phase=wait reason=actor_input_or_named_camera_lease_pending "
                "presentation=native_full_width\r\n");
        }
    } else if (!fixed_three_base_leases_exact()) {
        (void)fixed_three_release_cameras("lease_changed");
    }
    original_render_start();
    shared_modal_clear = refresh_shared_interaction_modal() ==
        SUDEKIMP_SHARED_INTERACTION_MODAL_NONE;
    base_leases_exact = fixed_three_runtime.cameras_acquired &&
        fixed_three_base_leases_exact();
    presentation_clear = base_leases_exact &&
        shared_modal_clear &&
        InterlockedCompareExchange(&native_save_modal_opening, 0, 0) == 0 &&
        !pc_quit_screen_visible() &&
        !quick_menu_visible() &&
        !genuine_quick_menu_visible() &&
        current_spirit_presentation_state() == 0 &&
        !settled_temporary_zone_active() &&
        !coop_roster_party_transition_active;
    allow_user_orbit = SudekiMpSplitScreenFixedThreeOrbitInputPolicy(
        presentation_clear,
        base_leases_exact,
        fixed_three_layout_exact(),
        fixed_three_runtime.frame_valid_mask,
        fixed_three_runtime.frame_owner_evidence_mask,
        SudekiMpControlSeparationGameplayInputFrozen());
    if (presentation_clear &&
        fixed_three_apply_render_seat(
            fixed_three_runtime.scheduled_seat,
            allow_user_orbit)) {
        fixed_three_runtime.rendered_seat =
            fixed_three_runtime.scheduled_seat;
        fixed_three_runtime.presentation_clear_this_frame = TRUE;
        rendered_player_two_this_frame =
            fixed_three_runtime.rendered_seat != 0u;
        viewport_hud_binding_active = TRUE;
        fixed_three_runtime.portrait_evidence_seat =
            fixed_three_runtime.rendered_seat;
        fixed_three_runtime.portrait_role_mask =
            refresh_viewport_portraits();
    } else {
        fixed_three_invalidate_frame_cache();
        fixed_three_runtime.rendered_seat = 0u;
        rendered_player_two_this_frame = FALSE;
        viewport_hud_binding_active = FALSE;
    }
    return TRUE;
}

static void fixed_three_frame_end_dispatch(void) {
    uint8_t next_seat = SUDEKIMP_LOCAL_VIEWPORT_NO_CONTROLLER;
    unsigned int attempted_seat = fixed_three_runtime.scheduled_seat <
            FIXED_THREE_SEAT_COUNT ?
        fixed_three_runtime.scheduled_seat : 0u;
    BOOL owner_evidence_ready = FALSE;
    BOOL restore_succeeded;
    BOOL capture_succeeded = FALSE;

    viewport_hud_binding_active = FALSE;
    restore_succeeded = fixed_three_restore_render_swap();
    original_frame_end();
    if (fixed_three_runtime.presentation_clear_this_frame &&
        fixed_three_runtime.rendered_seat == attempted_seat) {
        owner_evidence_ready = fixed_three_consume_frame_owner_evidence(
            fixed_three_runtime.rendered_seat);
    } else {
        fixed_three_reset_render_owner_evidence();
        fixed_three_clear_minimap_update_proof();
        fixed_three_runtime.minimap_expected_update_seat =
            SUDEKIMP_LOCAL_VIEWPORT_NO_CONTROLLER;
    }
    if (restore_succeeded &&
        fixed_three_runtime.presentation_clear_this_frame &&
        owner_evidence_ready &&
        fixed_three_assignment_selected() &&
        fixed_three_runtime.configured &&
        fixed_three_base_leases_exact()) {
        capture_succeeded = compose_fixed_three_camera_frames(
            fixed_three_runtime.rendered_seat,
            owner_evidence_ready);
    }
    if (!capture_succeeded) {
        fixed_three_invalidate_frame_cache();
        fixed_three_runtime.scheduled_seat = (uint8_t)attempted_seat;
    } else if (SudekiMpLocalViewportNextRenderSeat(
            FIXED_THREE_HUMAN_MASK,
            fixed_three_runtime.rendered_seat,
            FALSE,
            SUDEKIMP_LOCAL_VIEWPORT_NO_CONTROLLER,
            &next_seat)) {
        fixed_three_runtime.scheduled_seat = next_seat;
    } else {
        fixed_three_invalidate_frame_cache();
        fixed_three_runtime.scheduled_seat = 0u;
    }
    if (restore_succeeded &&
        fixed_three_runtime.presentation_clear_this_frame &&
        fixed_three_assignment_selected() &&
        fixed_three_runtime.configured &&
        fixed_three_runtime.cameras_acquired &&
        fixed_three_base_leases_exact()) {
        fixed_three_arm_minimap_update(
            fixed_three_runtime.scheduled_seat);
    }
    if (overlay_renderer != NULL) {
        overlay_renderer();
    }
    fixed_three_runtime.transaction_active = FALSE;
    fixed_three_runtime.presentation_clear_this_frame = FALSE;
    fixed_three_runtime.rendered_seat = 0u;
    rendered_player_two_this_frame = FALSE;
    genuine_quick_menu_active_this_frame = FALSE;
    quick_menu_live_player_two_available_this_frame = FALSE;
    quick_menu_render_phase_confirmed_this_frame = FALSE;
    quick_menu_submit_seen_since_frame_end = FALSE;
}

void SudekiMpSplitScreenRenderStartDispatch(void) {
    BOOL quit_menu_visible;
    BOOL quick_menu_is_visible;
    BOOL genuine_quick_menu_is_visible;
    BOOL genuine_quick_menu_rising_edge;
    BOOL spirit_presentation_active;
    BOOL spirit_state_active;
    BOOL isolation_in_progress;
    BOOL isolation_resources_ready;
    BOOL live_view_allowed;
    BOOL shared_modal_native_full_width;
    BOOL player_two_requested_before_apply;
    unsigned int isolation_state;
    unsigned int shared_modal_observation;
    BOOL runtime_authorized;
    void *trace_player_one_character = player_one_character;
    void *trace_player_two_character = player_two_character;
    void *resolved_player_one_character = NULL;
    void *resolved_player_two_character = NULL;
    unsigned int resolved_player_two_slot = 0u;

    ++split_render_frame_sequence;
    runtime_authorized = SudekiMpSplitScreenRuntimeAuthorized();
    runtime_authorized_at_render_start = runtime_authorized;
    /* This is the last guaranteed boundary before the native frame begins.
     * Quarantine closes here so no UI-tree teardown occurs inside the native
     * render transaction. */
    quick_menu_service_owner_session(genuine_quick_menu_visible());

    if (fixed_three_assignment_selected()) {
        (void)fixed_three_render_start_dispatch(runtime_authorized);
        return;
    }
    if (fixed_three_runtime.cameras_acquired) {
        (void)fixed_three_release_cameras("three_seat_assignment_inactive");
    }

    /*
     * Keep the ranged trace useful when the split-camera ownership is
     * temporarily released.  The native companion still has a world-side
     * animation controller while AI owns it; resolving the same party pair
     * here lets a read-only capture observe that controller without borrowing
     * a render wrapper or changing AI/input state.
     */
    if (runtime_authorized &&
        (trace_player_one_character == NULL ||
         trace_player_two_character == NULL)) {
        if (resolve_player_characters(
                &resolved_player_one_character,
                &resolved_player_two_character,
                &resolved_player_two_slot)) {
            if (trace_player_one_character == NULL) {
                trace_player_one_character = resolved_player_one_character;
            }
            if (trace_player_two_character == NULL) {
                trace_player_two_character = resolved_player_two_character;
            }
            if (player_one_character == NULL) {
                player_one_character = resolved_player_one_character;
            }
            if (player_two_character == NULL) {
                player_two_character = resolved_player_two_character;
            }
        }
    }

    rendered_player_two_this_frame = FALSE;
    viewport_hud_binding_active = FALSE;
    genuine_quick_menu_active_this_frame = FALSE;
    quick_menu_live_player_two_available_this_frame = FALSE;
    original_render_start();
    shared_modal_observation = refresh_shared_interaction_modal();
    shared_modal_native_full_width = shared_modal_observation !=
        SUDEKIMP_SHARED_INTERACTION_MODAL_NONE;
    quit_menu_visible = pc_quit_screen_visible();
    quick_menu_is_visible = quick_menu_visible();
    genuine_quick_menu_is_visible = genuine_quick_menu_visible();
    spirit_state_active = current_spirit_presentation_state() != 0;
    spirit_presentation_active =
        spirit_strike_viewport_effect_isolation_enabled &&
        spirit_state_active;
    genuine_quick_menu_active_this_frame =
        genuine_quick_menu_is_visible &&
        !quit_menu_visible &&
        !spirit_state_active;
    genuine_quick_menu_rising_edge =
        genuine_quick_menu_is_visible &&
        !quick_menu_genuine_visible_previous_frame;
    if (genuine_quick_menu_rising_edge) {
        /* An explicit P2 request captures its owner before UIStart.  Preserve
         * that pending lease across the native visible edge; only a native
         * no-request open is inferred from P1's unchanged controller target. */
        quick_menu_latch_owner_from_controller();
    }
    isolation_state = quick_menu_isolation_tail_active ?
        SUDEKIMP_QUICK_MENU_ISOLATION_TAIL :
        (quick_menu_isolation_active ?
            SUDEKIMP_QUICK_MENU_ISOLATION_ACTIVE :
            (quick_menu_isolation_failed_for_open_menu ?
                SUDEKIMP_QUICK_MENU_ISOLATION_FAILED :
                SUDEKIMP_QUICK_MENU_ISOLATION_IDLE));
    isolation_state = SudekiMpSplitScreenQuickMenuIsolationBeginState(
        isolation_state,
        quick_menu_genuine_visible_previous_frame,
        genuine_quick_menu_is_visible,
        genuine_quick_menu_active_this_frame &&
            quick_menu_live_player_two_ready() &&
            quick_menu_owner_session_valid
    );
    quick_menu_isolation_active =
        isolation_state == SUDEKIMP_QUICK_MENU_ISOLATION_ACTIVE;
    quick_menu_isolation_tail_active =
        isolation_state == SUDEKIMP_QUICK_MENU_ISOLATION_TAIL;
    quick_menu_isolation_failed_for_open_menu =
        isolation_state == SUDEKIMP_QUICK_MENU_ISOLATION_FAILED;
    if ((quit_menu_visible || spirit_state_active) &&
        (genuine_quick_menu_is_visible ||
         quick_menu_isolation_active ||
         quick_menu_isolation_tail_active)) {
        isolation_state =
            SudekiMpSplitScreenQuickMenuIsolationCancelState(
                genuine_quick_menu_is_visible
            );
        quick_menu_isolation_active =
            isolation_state == SUDEKIMP_QUICK_MENU_ISOLATION_ACTIVE;
        quick_menu_isolation_tail_active =
            isolation_state == SUDEKIMP_QUICK_MENU_ISOLATION_TAIL;
        quick_menu_isolation_failed_for_open_menu =
            isolation_state == SUDEKIMP_QUICK_MENU_ISOLATION_FAILED;
        quick_menu_expected_player_two = FALSE;
    }
    quick_menu_genuine_visible_previous_frame =
        genuine_quick_menu_is_visible;
    isolation_in_progress =
        quick_menu_isolation_active ||
        quick_menu_isolation_tail_active;
    isolation_resources_ready =
        isolation_in_progress && quick_menu_live_player_two_ready();
    if (isolation_in_progress && !isolation_resources_ready) {
        quick_menu_isolation_active = FALSE;
        quick_menu_isolation_tail_active = FALSE;
        quick_menu_isolation_failed_for_open_menu =
            genuine_quick_menu_is_visible;
        quick_menu_expected_player_two = FALSE;
        isolation_in_progress = FALSE;
    }
    quick_menu_expected_player_two =
        SudekiMpSplitScreenQuickMenuPinnedViewIsPlayerTwo(
            isolation_in_progress,
            quick_menu_owner_session_valid,
            quick_menu_owner_player_two,
            FALSE
        );
    if (isolation_in_progress) {
        player_two_view_requested = quick_menu_expected_player_two;
    } else if (genuine_quick_menu_is_visible) {
        player_two_view_requested = FALSE;
    } else {
        reset_quick_menu_owner_session();
    }
    if (player_two_temporary_camera_policy ==
            SUDEKIMP_TEMP_CAMERA_SHARED_FULL_WIDTH) {
        /* Fixed/unknown room cameras have one authored safe eye.  Render that
         * native owner once at full width rather than alternately labelling two
         * identical cached halves as independent views. */
        player_two_view_requested = FALSE;
    }
    if (shared_modal_native_full_width) {
        player_two_view_requested = FALSE;
    }
    if (!runtime_authorized) {
        player_two_view_requested = FALSE;
    }
    live_view_allowed = runtime_authorized &&
        !shared_modal_native_full_width &&
        !quit_menu_visible &&
        (isolation_in_progress ||
         (!genuine_quick_menu_is_visible &&
          (!quick_menu_is_visible || spirit_presentation_active)));
    if (!quit_menu_visible && !quick_menu_is_visible) {
        trace_ranged_animation_transition(0u, trace_player_one_character);
        trace_ranged_animation_transition(1u, trace_player_two_character);
        trace_ranged_animation_progress(0u, trace_player_one_character);
        trace_ranged_animation_progress(1u, trace_player_two_character);
    }
    player_two_requested_before_apply = player_two_view_requested;
    if (live_view_allowed) {
        apply_render_only_camera();
    }
    rendered_player_two_this_frame =
        live_view_allowed &&
        player_two_view_requested &&
        player_two_camera != NULL;
    trace_skill_camera_frame();
    quick_menu_render_phase_confirmed_this_frame = FALSE;
    if (isolation_in_progress) {
        quick_menu_render_phase_confirmed_this_frame =
            render_only_camera_phase_confirmed(
                quick_menu_expected_player_two
            );
        quick_menu_live_player_two_available_this_frame =
            SudekiMpSplitScreenQuickMenuLiveViewAccepted(
                isolation_in_progress,
                isolation_resources_ready,
                player_two_requested_before_apply,
                rendered_player_two_this_frame
            ) && quick_menu_render_phase_confirmed_this_frame;
        if (!quick_menu_live_player_two_available_this_frame) {
            restore_render_only_camera();
            quick_menu_isolation_active = FALSE;
            quick_menu_isolation_tail_active = FALSE;
            quick_menu_isolation_failed_for_open_menu =
                genuine_quick_menu_is_visible;
            quick_menu_expected_player_two = FALSE;
            player_two_view_requested = FALSE;
            rendered_player_two_this_frame = FALSE;
            quick_menu_render_phase_confirmed_this_frame = FALSE;
            live_view_allowed = !quit_menu_visible;
        }
    }
    if (!quit_menu_visible &&
        spirit_strike_viewport_effect_isolation_enabled &&
        routed_camera_effect_active()) {
        ensure_camera_history_resource();
    }
    trace_spirit_presentation(rendered_player_two_this_frame);
    compose_spirit_player_two_melee_locomotion();
    trace_spirit_player_two_presentation();
    if (live_view_allowed) {
        apply_ranged_model_render_view();
        (void)refresh_viewport_portraits();
        viewport_hud_binding_active = TRUE;
    }
}

void SudekiMpSplitScreenQuitScreenRenderDispatch(void) {
    if (!fixed_three_assignment_selected() && pc_quit_screen_visible()) {
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
    BOOL quick_menu_is_visible;
    BOOL presentation_allowed;
    BOOL isolation_in_progress;
    BOOL isolation_tail_active;
    BOOL cached_frame_advanced = TRUE;
    BOOL owner_capture_advanced = FALSE;
    BOOL isolation_cache_failed = FALSE;
    BOOL minimap_capture_allowed = TRUE;
    BOOL shared_temporary_full_width;
    BOOL shared_modal_native_full_width;
    BOOL recovery_eligible;
    BOOL recovery_fresh_pair_ready;
    const char *recovery_reason;
    unsigned int isolation_state;
    unsigned int shared_modal_observation;

    if (fixed_three_runtime.transaction_active) {
        fixed_three_frame_end_dispatch();
        return;
    }

    viewport_hud_binding_active = FALSE;
    restore_player_two_collision_self_cull();
    restore_ranged_model_render_view();
    restore_render_only_camera();
    original_frame_end();
    shared_modal_observation = refresh_shared_interaction_modal();
    shared_modal_native_full_width = shared_modal_observation !=
        SUDEKIMP_SHARED_INTERACTION_MODAL_NONE;
    /* Use the exact authorization sampled before RenderStart for the complete
     * native render transaction.  Resampling here could admit a false->true
     * half-frame or expose a raw Camera-2 frame on a true->false edge.  A
     * changed status takes effect at the next RenderStart. */
    split_allowed = gameplay_split_allowed(
        runtime_authorized_at_render_start,
        &gate_reason
    );
    quit_menu_visible = pc_quit_screen_visible();
    quick_menu_is_visible = quick_menu_visible();
    isolation_in_progress =
        quick_menu_isolation_active ||
        quick_menu_isolation_tail_active;
    isolation_tail_active = quick_menu_isolation_tail_active;
    presentation_allowed = split_allowed && !quit_menu_visible &&
        !shared_modal_native_full_width;
    shared_temporary_full_width =
        player_two_temporary_camera_policy ==
            SUDEKIMP_TEMP_CAMERA_SHARED_FULL_WIDTH;
    log_gameplay_gate(split_allowed, gate_reason);
    log_shared_menu_gate(quit_menu_visible);
    log_quick_menu_gate(
        genuine_quick_menu_active_this_frame,
        quick_menu_live_player_two_available_this_frame
    );
    if (presentation_allowed && !shared_temporary_full_width) {
        if (dual_camera_frame_cache_enabled) {
            minimap_capture_allowed =
                SudekiMpSplitScreenMinimapCaptureAllowed(
                    dual_camera_frame_cache_enabled,
                    minimap_update_owner_valid,
                    minimap_update_player_two,
                    rendered_player_two_this_frame
                ) && !minimap_source_failure_this_frame;
            if (!minimap_capture_allowed &&
                !minimap_owner_mismatch_logged) {
                const char *reason = minimap_source_failure_this_frame ?
                    "party_source_unavailable" :
                    (!minimap_update_owner_valid ?
                        "update_owner_unavailable" :
                        "update_owner_not_rendered");
                minimap_owner_mismatch_logged = TRUE;
                SudekiMpLogFormat(
                    "split_screen_render event=minimap_ownership phase=cache_hold reason=%s update_latch_valid=%u update_viewport=%u rendered_viewport=%u policy=retain_last_valid_camera_pair_without_wrong_owner_capture\r\n",
                    reason,
                    minimap_update_owner_valid ? 1u : 0u,
                    minimap_update_player_two ? 2u : 1u,
                    rendered_player_two_this_frame ? 2u : 1u
                );
            } else if (minimap_capture_allowed) {
                minimap_owner_mismatch_logged = FALSE;
            }
            cached_frame_advanced = compose_cached_camera_frames(
                rendered_player_two_this_frame,
                minimap_capture_allowed
            );
            owner_capture_advanced =
                SudekiMpSplitScreenQuickMenuOwnerCaptureAdvanced(
                    isolation_in_progress,
                    quick_menu_owner_session_valid,
                    quick_menu_owner_player_two,
                    rendered_player_two_this_frame,
                    minimap_capture_allowed,
                    cached_frame_advanced
                );
        } else {
            compose_native_frame();
        }
    }
    if (shared_interaction_modal_recovery_pending &&
        !shared_modal_native_full_width) {
        recovery_eligible =
            shared_interaction_recovery_runtime_eligible(
                FALSE,
                &recovery_reason
            );
        recovery_fresh_pair_ready =
            presentation_allowed && !shared_temporary_full_width &&
            cached_frame_advanced &&
            shared_interaction_recovery_runtime_eligible(TRUE, NULL);
        shared_interaction_modal_recovery_pending =
            SudekiMpSplitScreenSharedInteractionRecoveryPendingNext(
                TRUE,
                FALSE,
                FALSE,
                recovery_eligible,
                recovery_fresh_pair_ready
            );
        if (!shared_interaction_modal_recovery_pending) {
            if (recovery_fresh_pair_ready) {
                SudekiMpLogWrite(
                    "split_screen_render event=shared_interaction_modal phase=recovery_complete presentation=split_ready cache_policy=fresh_player_one_and_player_two_frames control_policy=resume\r\n"
                );
            } else {
                SudekiMpLogFormat(
                    "split_screen_render event=shared_interaction_modal phase=recovery_cancelled reason=%s policy=fail_open_do_not_rearm_on_rejoin\r\n",
                    recovery_reason
                );
            }
        }
    }
    isolation_cache_failed =
        isolation_in_progress && !cached_frame_advanced;
    if (isolation_cache_failed) {
        isolation_state =
            SudekiMpSplitScreenQuickMenuIsolationCancelState(
                genuine_quick_menu_visible()
            );
        quick_menu_isolation_active = FALSE;
        quick_menu_isolation_tail_active = FALSE;
        quick_menu_isolation_failed_for_open_menu =
            isolation_state == SUDEKIMP_QUICK_MENU_ISOLATION_FAILED;
        quick_menu_expected_player_two = FALSE;
        quick_menu_live_player_two_available_this_frame = FALSE;
        quick_menu_render_phase_confirmed_this_frame = FALSE;
        player_two_view_requested = FALSE;
        isolation_in_progress = FALSE;
        if (isolation_state == SUDEKIMP_QUICK_MENU_ISOLATION_IDLE) {
            reset_quick_menu_owner_session();
        }
        SudekiMpLogWrite(
            "split_screen_render event=quick_menu_live_isolation phase=fallback reason=camera_cache_capture_or_compose_failed policy=preserve_last_valid_camera_pair_for_remainder_of_menu\r\n"
        );
    }
    if (shared_modal_native_full_width) {
        quick_menu_expected_player_two = FALSE;
        player_two_view_requested = FALSE;
    } else if (split_allowed && isolation_in_progress &&
        quick_menu_live_player_two_available_this_frame &&
        quick_menu_render_phase_confirmed_this_frame &&
        quick_menu_live_player_two_ready()) {
        /* QuickMenu::Render mixes immediately presented UI with text queued
         * for the next CUIScene flush.  Keep both destinations on the owner
         * until the close tail observes a submit-free, successfully captured
         * owner frame.  The non-owner half intentionally retains its last
         * clean cached world frame for this short native modal session. */
        quick_menu_expected_player_two =
            SudekiMpSplitScreenQuickMenuPinnedViewIsPlayerTwo(
                TRUE,
                quick_menu_owner_session_valid,
                quick_menu_owner_player_two,
                FALSE
            );
        player_two_view_requested = quick_menu_expected_player_two;
        if (isolation_tail_active) {
            isolation_state =
                SudekiMpSplitScreenQuickMenuIsolationEndState(
                    SUDEKIMP_QUICK_MENU_ISOLATION_TAIL,
                    quick_menu_submit_seen_since_frame_end,
                    owner_capture_advanced
                );
            quick_menu_isolation_tail_active =
                isolation_state == SUDEKIMP_QUICK_MENU_ISOLATION_TAIL;
            if (!quick_menu_isolation_tail_active) {
                quick_menu_expected_player_two = FALSE;
                player_two_view_requested =
                    !rendered_player_two_this_frame;
                isolation_in_progress = FALSE;
                reset_quick_menu_owner_session();
                SudekiMpLogWrite(
                    "split_screen_render event=quick_menu_live_isolation phase=tail_complete reason=submit_free_owner_frame_captured policy=resume_opposite_viewport_alternation_preserve_clean_non_owner_cache\r\n"
                );
            }
        }
    } else if (isolation_cache_failed || isolation_in_progress) {
        quick_menu_isolation_active = FALSE;
        quick_menu_isolation_tail_active = FALSE;
        quick_menu_isolation_failed_for_open_menu =
            genuine_quick_menu_visible();
        quick_menu_expected_player_two = FALSE;
        player_two_view_requested = FALSE;
        if (!quick_menu_isolation_failed_for_open_menu) {
            reset_quick_menu_owner_session();
        }
    } else if (!quit_menu_visible && !quick_menu_is_visible &&
               !quick_menu_isolation_failed_for_open_menu) {
        poll_second_player_camera(split_allowed);
    } else if (quick_menu_isolation_failed_for_open_menu) {
        player_two_view_requested = FALSE;
    }
    latch_minimap_scheduled_owner();
    if (overlay_renderer != NULL) {
        overlay_renderer();
    }
    rendered_player_two_this_frame = FALSE;
    genuine_quick_menu_active_this_frame = FALSE;
    quick_menu_live_player_two_available_this_frame = FALSE;
    quick_menu_render_phase_confirmed_this_frame = FALSE;
    quick_menu_submit_seen_since_frame_end = FALSE;
}

__attribute__((naked, noinline, used))
static void split_screen_frame_end_entry(void) {
    __asm__ volatile(
        "call _SudekiMpSplitScreenFrameEndDispatch\n\t"
        "ret\n\t"
    );
}

BOOL SudekiMpSplitScreenSetRuntimeEnabled(BOOL enabled) {
    if (coop_role_lock_active && !enabled) {
        if (game_base != NULL) {
            SudekiMpLogWrite(
                "split_screen_render event=runtime_toggle status=rejected "
                "reason=co_op_roles_locked\r\n"
            );
        }
        SetLastError(ERROR_LOCK_VIOLATION);
        return FALSE;
    }
    runtime_split_enabled = enabled != FALSE;
    if (game_base != NULL) {
        SudekiMpLogFormat(
            "split_screen_render event=runtime_toggle state=%s\r\n",
            runtime_split_enabled ? "enabled" : "disabled"
        );
    }
    return TRUE;
}

BOOL SudekiMpSplitScreenRuntimeEnabled(void) {
    return runtime_split_enabled;
}

unsigned int SudekiMpSplitScreenRosterSeatCapacity(void) {
    if (!split_screen_render_installed) {
        return 0u;
    }
    return fixed_three_runtime.configured ? 3u : 2u;
}

BOOL SudekiMpSplitScreenSetFixedThreeSeatEnabled(BOOL enabled) {
    BOOL requested = enabled != FALSE;

    if (!split_screen_render_installed) {
        SetLastError(ERROR_INVALID_STATE);
        return FALSE;
    }
    if (fixed_three_runtime.transaction_active) {
        SetLastError(ERROR_BUSY);
        return FALSE;
    }
    if (requested &&
        (!second_player_camera_enabled ||
         !dual_camera_frame_cache_enabled ||
         !second_player_controller_camera_enabled ||
         camera_manager_add_camera == NULL ||
         camera_manager_remove_camera == NULL ||
         camera_manager_get_camera == NULL ||
         SudekiMpLocalInputHubRequestedMask() !=
            FIXED_THREE_HUMAN_MASK)) {
        SetLastError(ERROR_INVALID_STATE);
        return FALSE;
    }
    if (fixed_three_runtime.configured == requested) {
        return TRUE;
    }
    if (!requested &&
        !fixed_three_release_cameras("fixed_three_config_disabled")) {
        SetLastError(ERROR_BUSY);
        return FALSE;
    }
    fixed_three_runtime.configured = requested;
    fixed_three_invalidate_frame_cache();
    SudekiMpLogFormat(
        "split_screen_render event=fixed_three_seat_config state=%s "
        "active_mask=0x07 camera_names=%s,%s "
        "policy=explicit_opt_in_0x07_assignment_only_reverse_teardown\r\n",
        requested ? "enabled" : "disabled",
        second_player_camera_name,
        third_player_camera_name);
    return TRUE;
}

BOOL SudekiMpSplitScreenFixedThreeSeatEnabled(void) {
    return split_screen_render_installed &&
        fixed_three_runtime.configured;
}

void SudekiMpSplitScreenSetRuntimeAuthorizationQuery(
    SudekiMpSplitScreenRuntimeAuthorizationQuery query
) {
    runtime_authorization_query = query;
}

BOOL SudekiMpSplitScreenRuntimeAuthorizationPolicy(
    BOOL runtime_enabled,
    BOOL query_present,
    BOOL query_authorized
) {
    return runtime_enabled && (!query_present || query_authorized);
}

BOOL SudekiMpSplitScreenExternalPlayerTwoLeasePolicy(
    BOOL external_authorization_present,
    BOOL player_two_requested,
    BOOL player_two_active,
    const void *combat_context_character,
    const void *control_lease_character
) {
    return !external_authorization_present ||
        (player_two_requested && player_two_active &&
         combat_context_character != NULL &&
         combat_context_character == control_lease_character);
}

BOOL SudekiMpSplitScreenRuntimeAuthorized(void) {
    SudekiMpSplitScreenRuntimeAuthorizationQuery query =
        runtime_authorization_query;
    DWORD entry_error = GetLastError();
    BOOL authorized = TRUE;
    BOOL accepted;
    int state;

    if (query != NULL) {
        authorized = query();
    }
    accepted = SudekiMpSplitScreenRuntimeAuthorizationPolicy(
        runtime_split_enabled,
        query != NULL,
        authorized
    );
    state = accepted ? 1 : 0;
    if (state != runtime_authorization_last_state) {
        runtime_authorization_last_state = state;
        player_two_view_requested = FALSE;
        invalidate_dual_frame_cache();
        fixed_three_invalidate_frame_cache();
        if (game_base != NULL && query != NULL) {
            SudekiMpLogFormat(
                "split_screen_render event=external_runtime_gate state=%s "
                "policy=full_width_until_exact_authorization_then_two_fresh_frames\r\n",
                accepted ? "authorized" : "native_full_width"
            );
        }
    }
    SetLastError(entry_error);
    return accepted;
}

static BOOL lock_coop_roster_roles(
    void *player_one,
    void *player_two,
    void *player_three,
    BOOL require_player_three
) {
    if (player_one == NULL || player_two == NULL ||
        player_one == player_two ||
        !readable_memory(player_one, 0x94u) ||
        !readable_memory(player_two, 0x94u) ||
        (require_player_three &&
         (player_three == NULL || player_three == player_one ||
          player_three == player_two ||
          !readable_memory(player_three, 0x94u))) ||
        (!require_player_three && player_three != NULL)) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    coop_locked_player_one = player_one;
    coop_locked_player_two = player_two;
    coop_locked_player_three = require_player_three ? player_three : NULL;
    coop_role_lock_active = TRUE;
    SudekiMpLogFormat(
        "split_screen_render event=co_op_roles phase=locked "
        "active_mask=0x%02lx player_one=0x%08lx player_two=0x%08lx "
        "player_three=0x%08lx "
        "policy=immutable_party_identities_until_uninstall\r\n",
        (unsigned long)(require_player_three ?
            FIXED_THREE_HUMAN_MASK : 0x03u),
        (unsigned long)(uintptr_t)player_one,
        (unsigned long)(uintptr_t)player_two,
        (unsigned long)(uintptr_t)coop_locked_player_three
    );
    return TRUE;
}

BOOL SudekiMpSplitScreenLockRoles(void *player_one, void *player_two) {
    return lock_coop_roster_roles(
        player_one, player_two, NULL, FALSE);
}

BOOL SudekiMpSplitScreenRolesLocked(void) {
    return coop_role_lock_active;
}

BOOL SudekiMpSplitScreenSetRosterTypes(
    unsigned int player_one_type,
    unsigned int player_two_type
) {
    SudekiMpCoopRosterAssignment assignment;

    ZeroMemory(&assignment, sizeof(assignment));
    assignment.active_human_mask = 0x03u;
    assignment.actor_type_by_seat[0] = player_one_type;
    assignment.actor_type_by_seat[1] = player_two_type;
    /* The legacy title path may publish its two-seat contract immediately
     * before the render hook is installed.  It is still validated against
     * the exact capacity-two contract; the public capacity query correctly
     * remains zero until the renderer itself is available. */
    if (!SudekiMpCoopRosterAssignmentStoreCommit(
            &coop_roster_assignment_store, &assignment, 2u)) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    coop_roster_player_one_type = player_one_type;
    coop_roster_player_two_type = player_two_type;
    coop_roster_valid = TRUE;
    coop_roster_participation_requested = TRUE;
    coop_roster_party_transition_active = FALSE;
    coop_roster_runtime_release_pending = FALSE;
    coop_roster_last_presence_mask = ~0u;
    coop_roster_ready_window_active = FALSE;
    coop_roster_ready_since = 0u;
    coop_roster_ready_group = NULL;
    coop_roster_ready_controller_target = NULL;
    coop_roster_ready_party_front = NULL;
    coop_roster_ready_player_one = NULL;
    coop_roster_ready_player_two = NULL;
    coop_roster_ready_player_three = NULL;
    coop_roster_ready_player_one_slot = 0u;
    coop_roster_ready_player_two_slot = 0u;
    coop_roster_ready_player_three_slot = 0u;
    coop_roster_ready_party_count = 0;
    coop_roster_control_phase = -1;
    SudekiMpLogFormat(
        "split_screen_render event=co_op_roster phase=selected "
        "active_mask=0x03 player_one_type=0x%02lx player_two_type=0x%02lx "
        "policy=persist_until_process_exit\r\n",
        (unsigned long)player_one_type,
        (unsigned long)player_two_type
    );
    return TRUE;
}

BOOL SudekiMpSplitScreenSetRosterAssignment(
    const SudekiMpCoopRosterAssignment *assignment
) {
    unsigned int capacity = SudekiMpSplitScreenRosterSeatCapacity();

    if (assignment == NULL || capacity == 0u ||
        (assignment->active_human_mask != 0x03u &&
         assignment->active_human_mask != FIXED_THREE_HUMAN_MASK) ||
        !SudekiMpCoopRosterAssignmentStoreCommit(
            &coop_roster_assignment_store, assignment, capacity)) {
        SetLastError(capacity == 0u ?
            ERROR_INVALID_STATE : ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    coop_roster_player_one_type = assignment->actor_type_by_seat[0];
    coop_roster_player_two_type = assignment->actor_type_by_seat[1];
    coop_roster_valid = TRUE;
    coop_roster_participation_requested = TRUE;
    coop_roster_party_transition_active = FALSE;
    coop_roster_runtime_release_pending = FALSE;
    coop_roster_last_presence_mask = ~0u;
    reset_coop_roster_ready_window();
    coop_roster_control_phase = -1;
    fixed_three_invalidate_frame_cache();
    SudekiMpLogFormat(
        "split_screen_render event=co_op_roster phase=selected "
        "active_mask=0x%02lx p1_type=0x%02lx p2_type=0x%02lx "
        "p3_type=0x%02lx capacity=%u "
        "policy=transactional_seat_indexed_assignment\r\n",
        (unsigned long)assignment->active_human_mask,
        (unsigned long)assignment->actor_type_by_seat[0],
        (unsigned long)assignment->actor_type_by_seat[1],
        (unsigned long)assignment->actor_type_by_seat[2],
        capacity);
    return TRUE;
}

BOOL SudekiMpSplitScreenGetRosterAssignment(
    SudekiMpCoopRosterAssignment *assignment
) {
    if (assignment == NULL) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    if (!coop_roster_valid ||
        !SudekiMpCoopRosterAssignmentStoreGet(
            &coop_roster_assignment_store, assignment)) {
        SetLastError(ERROR_NOT_FOUND);
        return FALSE;
    }
    return TRUE;
}

static BOOL release_coop_control_seats_reverse(void) {
    BOOL player_three_released;
    BOOL player_two_released;

    /* Control separation deliberately refuses to release P2 while P3 still
     * depends on it. Always unwind the dependency in reverse seat order. */
    player_three_released =
        SudekiMpControlSeparationReleaseSeatNow(2u);
    player_two_released =
        SudekiMpControlSeparationReleaseSeatNow(1u);
    return player_three_released && player_two_released;
}

static BOOL coop_control_seats_present(void) {
    return SudekiMpControlSeparationSeatRequested(1u) ||
        SudekiMpControlSeparationSeatCharacter(1u) != NULL ||
        SudekiMpControlSeparationSeatRequested(2u) ||
        SudekiMpControlSeparationSeatCharacter(2u) != NULL;
}

static BOOL release_coop_roster_runtime(const char *reason) {
    BOOL fixed_cameras_released;
    BOOL legacy_camera_released;
    BOOL camera_released;
    BOOL control_released;

    fixed_cameras_released = fixed_three_release_cameras(
        reason == NULL ? "roster_runtime_release" : reason);
    legacy_camera_released = release_player_two_camera(
        reason == NULL ? "roster_runtime_release" : reason);
    camera_released = fixed_cameras_released && legacy_camera_released;
    if (!camera_released) {
        /* A named/render-only camera still holds actor-facing view state.
         * Preserve role/control/runtime ownership and retry at a later safe
         * game-thread boundary; dropping the actor lease first would strand
         * a live camera on a released AI character. */
        reset_coop_roster_ready_window();
        coop_roster_control_phase = 2;
        coop_roster_runtime_release_pending = TRUE;
        SudekiMpLogFormat(
            "split_screen_render event=co_op_roster phase=runtime_release "
            "reason=%s camera=pending fixed_camera=%s legacy_camera=%s "
            "control=retained policy=camera_release_before_role_or_actor_release\r\n",
            reason == NULL ? "unspecified" : reason,
            fixed_cameras_released ? "released" : "pending",
            legacy_camera_released ? "released" : "pending");
        return FALSE;
    }
    coop_role_lock_active = FALSE;
    coop_locked_player_one = NULL;
    coop_locked_player_two = NULL;
    coop_locked_player_three = NULL;
    (void)SudekiMpControlSeparationSetRoleLock(FALSE);
    control_released = release_coop_control_seats_reverse();
    (void)SudekiMpSplitScreenSetRuntimeEnabled(FALSE);
    reset_coop_roster_ready_window();
    coop_roster_control_phase = -1;
    coop_roster_runtime_release_pending =
        !(camera_released && control_released);
    SudekiMpLogFormat(
        "split_screen_render event=co_op_roster phase=runtime_release "
        "reason=%s camera=%s fixed_camera=%s legacy_camera=%s control=%s "
        "policy=retain_selected_types_restore_native_ai_p3_then_p2\r\n",
        reason == NULL ? "unspecified" : reason,
        camera_released ? "released" : "pending",
        fixed_cameras_released ? "released" : "pending",
        legacy_camera_released ? "released" : "pending",
        control_released ? "released" : "pending");
    return camera_released && control_released;
}

BOOL SudekiMpSplitScreenClearRosterTypes(void) {
    if (coop_roster_party_transition_active) {
        SetLastError(ERROR_BUSY);
        return FALSE;
    }
    if (coop_roster_valid && game_base != NULL &&
        !release_coop_roster_runtime("roster_contract_cleared")) {
        SetLastError(ERROR_BUSY);
        return FALSE;
    }
    coop_roster_valid = FALSE;
    coop_roster_player_one_type = 0u;
    coop_roster_player_two_type = 0u;
    SudekiMpCoopRosterAssignmentStoreInitialize(
        &coop_roster_assignment_store);
    fixed_three_invalidate_frame_cache();
    coop_roster_participation_requested = FALSE;
    coop_roster_party_transition_active = FALSE;
    coop_roster_runtime_release_pending = FALSE;
    reset_coop_roster_ready_window();
    coop_roster_control_phase = -1;
    SudekiMpLogWrite(
        "split_screen_render event=co_op_roster phase=cleared "
        "policy=single_player_has_no_multiplayer_runtime_contract\r\n");
    return TRUE;
}

BOOL SudekiMpSplitScreenRosterParticipationAvailable(void) {
    return coop_roster_valid && game_base != NULL;
}

BOOL SudekiMpSplitScreenRosterParticipationRequested(void) {
    return coop_roster_valid && coop_roster_participation_requested;
}

BOOL SudekiMpSplitScreenRequestRosterParticipation(BOOL enabled) {
    BOOL requested = enabled != FALSE;

    if (!coop_roster_valid || game_base == NULL) {
        SetLastError(ERROR_INVALID_STATE);
        return FALSE;
    }
    if (coop_roster_party_transition_active) {
        SetLastError(ERROR_BUSY);
        return FALSE;
    }
    if (requested && coop_roster_runtime_release_pending) {
        SetLastError(ERROR_BUSY);
        return FALSE;
    }
    if (coop_roster_participation_requested == requested) {
        return TRUE;
    }
    coop_roster_participation_requested = requested;
    if (!requested) {
        coop_roster_party_transition_active = FALSE;
        (void)release_coop_roster_runtime("player_two_drop_out");
    } else {
        reset_coop_roster_ready_window();
        coop_roster_control_phase = -1;
    }
    SudekiMpLogFormat(
        "split_screen_render event=co_op_roster phase=participation "
        "player_two=%s policy=drop_out_restores_ai_drop_in_reclaims_locked_character\r\n",
        requested ? "drop_in_requested" : "dropped_out");
    return TRUE;
}

BOOL SudekiMpSplitScreenBeginPartyTransition(void) {
    if (!coop_roster_valid || game_base == NULL) {
        SetLastError(ERROR_INVALID_STATE);
        return FALSE;
    }
    if (coop_roster_party_transition_active) {
        SetLastError(ERROR_BUSY);
        return FALSE;
    }
    coop_roster_party_transition_active = TRUE;
    if (!release_coop_roster_runtime("party_transition_begin")) {
        SudekiMpLogWrite(
            "split_screen_render event=co_op_roster "
            "phase=party_transition_quarantine status=pending "
            "policy=native_transition_may_continue_split_remains_disabled\r\n");
    }
    SudekiMpLogWrite(
        "split_screen_render event=co_op_roster phase=party_transition_begin "
        "policy=single_native_world_release_camera_and_p2_control\r\n");
    return TRUE;
}

BOOL SudekiMpSplitScreenEndPartyTransition(BOOL party_placed) {
    if (!coop_roster_party_transition_active) {
        SetLastError(ERROR_INVALID_STATE);
        return FALSE;
    }
    if (party_placed && coop_roster_runtime_release_pending) {
        SetLastError(ERROR_BUSY);
        SudekiMpLogWrite(
            "split_screen_render event=co_op_roster "
            "phase=party_transition_end status=deferred "
            "reason=runtime_release_not_confirmed\r\n");
        return FALSE;
    }
    coop_roster_party_transition_active = FALSE;
    if (!party_placed) {
        coop_roster_participation_requested = FALSE;
    }
    reset_player_two_controller_camera();
    invalidate_dual_frame_cache();
    reset_minimap_owner_schedule();
    reset_coop_roster_ready_window();
    coop_roster_control_phase = -1;
    SudekiMpLogFormat(
        "split_screen_render event=co_op_roster phase=party_transition_end "
        "party_placed=%s next=%s policy=reacquire_only_after_verified_group_placement\r\n",
        party_placed ? "true" : "false",
        party_placed && coop_roster_participation_requested ?
            "reclaim_player_two" : "remain_dropped_out");
    return TRUE;
}

BOOL SudekiMpSplitScreenGetRosterTypes(
    unsigned int *player_one_type,
    unsigned int *player_two_type
) {
    SudekiMpCoopRosterAssignment assignment;

    if (player_one_type == NULL || player_two_type == NULL) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    if (!coop_roster_valid ||
        !SudekiMpCoopRosterAssignmentStoreGet(
            &coop_roster_assignment_store, &assignment) ||
        assignment.active_human_mask != 0x03u) {
        SetLastError(ERROR_NOT_FOUND);
        return FALSE;
    }
    *player_one_type = assignment.actor_type_by_seat[0];
    *player_two_type = assignment.actor_type_by_seat[1];
    return TRUE;
}

BOOL SudekiMpSplitScreenRosterLeadReady(
    const void *controller_target,
    const void *party_front,
    const void *expected_player_one,
    unsigned int controller_mode_80,
    unsigned int controller_mode_84,
    unsigned int group_state_d0,
    unsigned int group_switching_d6,
    unsigned int group_state_d7,
    unsigned int next_character_action,
    unsigned int previous_character_action
) {
    return expected_player_one != NULL &&
        controller_target == expected_player_one &&
        party_front == expected_player_one &&
        coop_roster_native_transition_settled(
            controller_mode_80,
            controller_mode_84,
            group_state_d0,
            group_switching_d6,
            group_state_d7,
            next_character_action,
            previous_character_action);
}

BOOL SudekiMpSplitScreenRosterLockHealthy(
    const void *controller_target,
    const void *party_front,
    const void *locked_player_one,
    const void *controlled_player_two,
    const void *locked_player_two
) {
    return locked_player_one != NULL && locked_player_two != NULL &&
        controller_target == locked_player_one &&
        party_front == locked_player_one &&
        controlled_player_two == locked_player_two;
}

BOOL SudekiMpSplitScreenRosterThreeSeatLockHealthy(
    const void *controller_target,
    const void *party_front,
    const void *locked_player_one,
    const void *controlled_player_two,
    const void *locked_player_two,
    const void *controlled_player_three,
    const void *locked_player_three
) {
    return SudekiMpSplitScreenRosterLockHealthy(
            controller_target,
            party_front,
            locked_player_one,
            controlled_player_two,
            locked_player_two) &&
        locked_player_three != NULL &&
        locked_player_three != locked_player_one &&
        locked_player_three != locked_player_two &&
        controlled_player_three == locked_player_three;
}

static void *find_roster_character(
    uint8_t *group,
    unsigned int expected_type,
    unsigned int party_count,
    unsigned int *slot_result
) {
    unsigned int index;
    void *match = NULL;
    unsigned int match_slot = 0u;

    if (group == NULL || expected_type == 0u || party_count == 0u ||
        party_count > PARTY_SLOT_COUNT ||
        !readable_memory(group + PARTY_COUNT_OFFSET, sizeof(int))) {
        return NULL;
    }
    for (index = 0u; index < party_count; ++index) {
        void *candidate = *(void **)(
            group + PARTY_SLOT_ZERO_OFFSET + index * PARTY_SLOT_STRIDE
        );
        if (candidate != NULL && character_has_resource_type(
                candidate, expected_type)) {
            if (match != NULL) {
                return NULL;
            }
            match = candidate;
            match_slot = index;
        }
    }
    if (match != NULL && slot_result != NULL) {
        *slot_result = match_slot;
    }
    return match;
}

BOOL SudekiMpSplitScreenRosterActorIdentityPolicy(
    unsigned int player_index,
    const void *actor,
    unsigned int requested_type,
    unsigned int expected_type,
    const void *unique_party_actor,
    unsigned int actor_occurrences,
    const void *locked_actor,
    BOOL runtime_enabled,
    BOOL roles_locked,
    BOOL participation_requested,
    BOOL party_transition_active
) {
    return player_index < FIXED_THREE_SEAT_COUNT && actor != NULL &&
        requested_type != 0u &&
        requested_type == expected_type && actor_occurrences == 1u &&
        unique_party_actor == actor && locked_actor == actor &&
        runtime_enabled && roles_locked && participation_requested &&
        !party_transition_active;
}

BOOL SudekiMpSplitScreenRosterActorIdentityMatches(
    unsigned int player_index,
    const void *actor,
    unsigned int character_type
) {
    SudekiMpCoopRosterAssignment assignment;
    uint8_t *group;
    unsigned int expected_type;
    const void *locked_actor;
    const void *unique_party_actor = NULL;
    unsigned int actor_occurrences = 0u;
    unsigned int index;
    int party_count;

    if (player_index >= FIXED_THREE_SEAT_COUNT || game_base == NULL ||
        !coop_roster_valid ||
        !SudekiMpCoopRosterAssignmentStoreGet(
            &coop_roster_assignment_store, &assignment) ||
        (assignment.active_human_mask & (1u << player_index)) == 0u ||
        !readable_memory(game_base + RVA_ACTIVE_GROUP_GLOBAL,
            sizeof(group))) {
        return FALSE;
    }
    expected_type = assignment.actor_type_by_seat[player_index];
    locked_actor = player_index == 0u ? coop_locked_player_one :
        (player_index == 1u ? coop_locked_player_two :
            coop_locked_player_three);
    group = *(uint8_t **)(game_base + RVA_ACTIVE_GROUP_GLOBAL);
    if (!readable_memory(group, PARTY_COUNT_OFFSET + sizeof(party_count))) {
        return FALSE;
    }
    party_count = *(int *)(group + PARTY_COUNT_OFFSET);
    if (party_count < 1 || party_count > (int)PARTY_SLOT_COUNT ||
        !readable_memory(group + PARTY_SLOT_ZERO_OFFSET,
            (size_t)party_count * PARTY_SLOT_STRIDE)) {
        return FALSE;
    }
    for (index = 0u; index < (unsigned int)party_count; ++index) {
        void *candidate = *(void **)(group + PARTY_SLOT_ZERO_OFFSET +
            index * PARTY_SLOT_STRIDE);
        if (candidate == actor) {
            unique_party_actor = candidate;
            ++actor_occurrences;
        }
    }
    return SudekiMpSplitScreenRosterActorIdentityPolicy(
        player_index,
        actor,
        character_type,
        expected_type,
        unique_party_actor,
        actor_occurrences,
        locked_actor,
        runtime_split_enabled,
        coop_role_lock_active,
        coop_roster_participation_requested,
        coop_roster_party_transition_active);
}

static void reset_coop_roster_ready_window(void) {
    coop_roster_ready_window_active = FALSE;
    coop_roster_ready_since = 0u;
    coop_roster_ready_group = NULL;
    coop_roster_ready_controller_target = NULL;
    coop_roster_ready_party_front = NULL;
    coop_roster_ready_player_one = NULL;
    coop_roster_ready_player_two = NULL;
    coop_roster_ready_player_three = NULL;
    coop_roster_ready_player_one_slot = 0u;
    coop_roster_ready_player_two_slot = 0u;
    coop_roster_ready_player_three_slot = 0u;
    coop_roster_ready_party_count = 0;
}

static BOOL coop_roster_ready_window_elapsed(
    uint8_t *group,
    int party_count,
    void *controller_target,
    void *party_front,
    void *desired_player_one,
    unsigned int desired_player_one_slot,
    void *desired_player_two,
    unsigned int desired_player_two_slot,
    void *desired_player_three,
    unsigned int desired_player_three_slot
) {
    if (!coop_roster_ready_window_active ||
        coop_roster_ready_group != group ||
        coop_roster_ready_party_count != party_count ||
        coop_roster_ready_controller_target != controller_target ||
        coop_roster_ready_party_front != party_front ||
        coop_roster_ready_player_one != desired_player_one ||
        coop_roster_ready_player_one_slot != desired_player_one_slot ||
        coop_roster_ready_player_two != desired_player_two ||
        coop_roster_ready_player_two_slot != desired_player_two_slot ||
        coop_roster_ready_player_three != desired_player_three ||
        coop_roster_ready_player_three_slot != desired_player_three_slot) {
        coop_roster_ready_window_active = TRUE;
        coop_roster_ready_since = GetTickCount();
        coop_roster_ready_group = group;
        coop_roster_ready_party_count = party_count;
        coop_roster_ready_controller_target = controller_target;
        coop_roster_ready_party_front = party_front;
        coop_roster_ready_player_one = desired_player_one;
        coop_roster_ready_player_one_slot = desired_player_one_slot;
        coop_roster_ready_player_two = desired_player_two;
        coop_roster_ready_player_two_slot = desired_player_two_slot;
        coop_roster_ready_player_three = desired_player_three;
        coop_roster_ready_player_three_slot = desired_player_three_slot;
        return FALSE;
    }
    return (DWORD)(GetTickCount() - coop_roster_ready_since) >=
        COOP_ROSTER_READY_WINDOW_MS;
}

static BOOL coop_roster_native_transition_settled(
    unsigned int controller_mode_80,
    unsigned int controller_mode_84,
    unsigned int group_state_d0,
    unsigned int group_switching_d6,
    unsigned int group_state_d7,
    unsigned int next_character_action,
    unsigned int previous_character_action
) {
    return controller_mode_80 == 1u && controller_mode_84 == 1u &&
        group_state_d0 == 0u && group_switching_d6 != 0u &&
        group_state_d7 == 0u && next_character_action == 0u &&
        previous_character_action == 0u;
}

/* The exact native Previous/Next consumers use ESI for CGroupPlayers and no
 * stack arguments.  Calling the consumer keeps Sudeki's own party rotation,
 * control reassignment, AI/arbiter transitions, and listener notifications;
 * calling the lower slot-rotation helpers directly would omit those effects. */
__attribute__((naked, noinline, used))
static void call_group_players_character_switch(
    void *group __attribute__((unused)),
    void *target __attribute__((unused))
) {
    __asm__ volatile(
        "pushl %esi\n\t"
        "movl 8(%esp), %esi\n\t"
        "call *12(%esp)\n\t"
        "popl %esi\n\t"
        "ret\n\t"
    );
}

static BOOL release_coop_control_for_retry(const char *reason) {
    BOOL released;

    (void)SudekiMpControlSeparationSetRoleLock(FALSE);
    released = release_coop_control_seats_reverse();
    reset_coop_roster_ready_window();
    coop_roster_control_phase = released ? -1 : 2;
    if (!released || reason != NULL) {
        SudekiMpLogFormat(
            "split_screen_render event=co_op_roster "
            "phase=control_cleanup status=%s reason=%s "
            "policy=release_p3_then_p2_before_retry\r\n",
            released ? "confirmed" : "pending",
            reason == NULL ? "unspecified" : reason);
    }
    return released;
}

void SudekiMpSplitScreenApplyRosterOnGameThread(void) {
    SudekiMpCoopRosterAssignment assignment;
    uint8_t *group;
    uint8_t *controller;
    void *controller_target;
    void *party_front;
    void *desired_player_one;
    void *desired_player_two;
    void *desired_player_three = NULL;
    unsigned int desired_slot;
    unsigned int desired_player_two_slot;
    unsigned int desired_player_three_slot = 0u;
    unsigned int controller_mode_80;
    unsigned int controller_mode_84;
    unsigned int group_state_d0;
    unsigned int group_switching_d6;
    unsigned int group_state_d7;
    unsigned int next_character_action;
    unsigned int previous_character_action;
    uint32_t actor_flags_50;
    uint32_t actor_flags_60;
    uint8_t *character_switch_actor;
    uint8_t *character_switch_ui_gate;
    BOOL fixed_three_assignment;
    int party_count;

    if (!coop_roster_valid || game_base == NULL ||
        !SudekiMpCoopRosterAssignmentStoreGet(
            &coop_roster_assignment_store, &assignment) ||
        (assignment.active_human_mask != 0x03u &&
         assignment.active_human_mask != FIXED_THREE_HUMAN_MASK) ||
        !readable_memory(game_base + RVA_ACTIVE_GROUP_GLOBAL,
            sizeof(group)) ||
        !readable_memory(game_base + RVA_CHARACTER_CONTROLLER_GLOBAL,
            sizeof(controller))) {
        return;
    }
    fixed_three_assignment = assignment.active_human_mask ==
        FIXED_THREE_HUMAN_MASK;
    if (fixed_three_assignment && !fixed_three_runtime.configured) {
        if (coop_role_lock_active || coop_control_seats_present() ||
            fixed_three_runtime.cameras_acquired ||
            fixed_three_runtime.transaction_active) {
            (void)release_coop_roster_runtime(
                "fixed_three_renderer_not_configured");
        }
        return;
    }
    if (coop_roster_party_transition_active ||
        !coop_roster_participation_requested) {
        if (coop_role_lock_active ||
            coop_control_seats_present() ||
            runtime_split_enabled || coop_roster_runtime_release_pending ||
            player_two_camera != NULL || render_only_swap_active ||
            fixed_three_runtime.cameras_acquired ||
            fixed_three_runtime.transaction_active) {
            (void)release_coop_roster_runtime(
                coop_roster_party_transition_active ?
                    "party_transition_wait" : "player_two_dropped_out");
        }
        return;
    }
    if (coop_roster_runtime_release_pending) {
        (void)release_coop_roster_runtime("runtime_release_retry");
        return;
    }
    group = *(uint8_t **)(game_base + RVA_ACTIVE_GROUP_GLOBAL);
    controller = *(uint8_t **)(game_base + RVA_CHARACTER_CONTROLLER_GLOBAL);
    if (!readable_memory(group, PARTY_STATE_D7_OFFSET + sizeof(uint8_t)) ||
        !readable_memory(controller, CONTROLLER_TARGET_OFFSET +
            sizeof(controller_target))) {
        return;
    }
    party_count = *(int *)(group + PARTY_COUNT_OFFSET);
    if (party_count < 1 || party_count > (int)PARTY_SLOT_COUNT) {
        return;
    }
    controller_target = *(void **)(controller + CONTROLLER_TARGET_OFFSET);
    party_front = *(void **)(group + PARTY_SLOT_ZERO_OFFSET);
    controller_mode_80 = *(unsigned int *)(
        controller + CONTROLLER_MODE_80_OFFSET);
    controller_mode_84 = *(unsigned int *)(
        controller + CONTROLLER_MODE_84_OFFSET);
    group_state_d0 = *(uint32_t *)(group + PARTY_STATE_D0_OFFSET);
    group_switching_d6 = *(uint8_t *)(group + PARTY_SWITCHING_D6_OFFSET);
    group_state_d7 = *(uint8_t *)(group + PARTY_STATE_D7_OFFSET);
    next_character_action = *(uint32_t *)(
        controller + CONTROLLER_NEXT_CHARACTER_OFFSET);
    previous_character_action = *(uint32_t *)(
        controller + CONTROLLER_PREVIOUS_CHARACTER_OFFSET);
    desired_player_one = find_roster_character(
        group, assignment.actor_type_by_seat[0],
        (unsigned int)party_count,
        &desired_slot
    );
    desired_player_two = find_roster_character(
        group, assignment.actor_type_by_seat[1],
        (unsigned int)party_count,
        &desired_player_two_slot
    );
    if (fixed_three_assignment) {
        desired_player_three = find_roster_character(
            group, assignment.actor_type_by_seat[2],
            (unsigned int)party_count,
            &desired_player_three_slot
        );
    }
    {
        unsigned int presence_mask =
            (desired_player_one != NULL ? 1u : 0u) |
            (desired_player_two != NULL ? 2u : 0u) |
            (desired_player_three != NULL ? 4u : 0u);
        if (presence_mask != coop_roster_last_presence_mask) {
            coop_roster_last_presence_mask = presence_mask;
            reset_coop_roster_ready_window();
            coop_roster_control_phase = -1;
            SudekiMpLogFormat(
                "split_screen_render event=co_op_roster phase=availability "
                "player_one=%s player_two=%s player_three=%s "
                "mask=0x%02lx expected_mask=0x%02lx "
                "policy=deferred_until_selected_party_members_exist\r\n",
                desired_player_one != NULL ? "present" : "waiting",
                desired_player_two != NULL ? "present" : "waiting",
                desired_player_three != NULL ? "present" :
                    (fixed_three_assignment ? "waiting" : "inactive"),
                (unsigned long)presence_mask,
                (unsigned long)assignment.active_human_mask);
        }
    }
    if (coop_role_lock_active) {
        BOOL identities_healthy = fixed_three_assignment ?
            SudekiMpSplitScreenRosterThreeSeatLockHealthy(
                controller_target,
                party_front,
                coop_locked_player_one,
                SudekiMpControlSeparationSeatCharacter(1u),
                coop_locked_player_two,
                SudekiMpControlSeparationSeatCharacter(2u),
                coop_locked_player_three) :
            SudekiMpSplitScreenRosterLockHealthy(
                controller_target,
                party_front,
                coop_locked_player_one,
                SudekiMpControlSeparationSeatCharacter(1u),
                coop_locked_player_two);
        BOOL player_two_lease_healthy =
            SudekiMpControlSeparationSeatRequested(1u) &&
            SudekiMpControlSeparationSeatActive(1u) &&
            (fixed_three_assignment ?
                SudekiMpControlSeparationSeatInputLeaseActive(1u) :
                SudekiMpControlSeparationSeatInputReady(1u));
        BOOL player_three_lease_healthy = fixed_three_assignment ?
            (SudekiMpControlSeparationSeatRequested(2u) &&
             SudekiMpControlSeparationSeatActive(2u) &&
             SudekiMpControlSeparationSeatInputLeaseActive(2u)) :
            (!SudekiMpControlSeparationSeatRequested(2u) &&
             SudekiMpControlSeparationSeatCharacter(2u) == NULL);

        if (identities_healthy && player_two_lease_healthy &&
            player_three_lease_healthy &&
            desired_player_one == coop_locked_player_one &&
            desired_player_two == coop_locked_player_two &&
            desired_player_three == coop_locked_player_three) {
            return;
        }
        /* A level transition can rebuild party objects even when the
         * selected character identities are unchanged.  Release only the
         * runtime pointer lock; the roster types remain the session contract
         * and will be applied again once both actors are available. */
        (void)release_coop_roster_runtime(
            "party_members_rebuilt_unavailable_or_control_lost");
        SudekiMpLogFormat(
            "split_screen_render event=co_op_roster phase=deferred "
            "reason=party_members_rebuilt_unavailable_or_control_lost "
            "controller_target=0x%08lx party_front=0x%08lx "
            "desired_player_one=0x%08lx desired_player_two=0x%08lx "
            "desired_player_three=0x%08lx "
            "policy=release_old_p3_then_p2_then_reacquire_selected_identities\r\n",
            (unsigned long)(uintptr_t)controller_target,
            (unsigned long)(uintptr_t)party_front,
            (unsigned long)(uintptr_t)desired_player_one,
            (unsigned long)(uintptr_t)desired_player_two,
            (unsigned long)(uintptr_t)desired_player_three);
        return;
    }
    if (desired_player_one == NULL || desired_player_two == NULL ||
        (fixed_three_assignment && desired_player_three == NULL)) {
        if (coop_control_seats_present()) {
            (void)release_coop_control_for_retry(
                "selected_actor_unavailable");
        }
        reset_coop_roster_ready_window();
        return; /* Selected characters may join the party later. */
    }
    if ((!SudekiMpControlSeparationSeatRequested(1u) &&
         SudekiMpControlSeparationSeatCharacter(1u) != NULL) ||
        (!SudekiMpControlSeparationSeatRequested(2u) &&
         SudekiMpControlSeparationSeatCharacter(2u) != NULL)) {
        /* A prior rollback is still restoring the native AI lease.  Never
         * rotate or reacquire until the old override is actually gone. */
        (void)release_coop_control_for_retry("release_pending");
        if (coop_roster_control_phase != 2) {
            coop_roster_control_phase = 2;
            SudekiMpLogWrite(
                "split_screen_render event=co_op_roster "
                "phase=control_cleanup reason=release_pending\r\n");
        }
        return;
    }
    if (!coop_roster_native_transition_settled(
            controller_mode_80,
            controller_mode_84,
            group_state_d0,
            group_switching_d6,
            group_state_d7,
            next_character_action,
        previous_character_action)) {
        reset_coop_roster_ready_window();
        if (coop_control_seats_present()) {
            (void)release_coop_control_for_retry(
                "native_transition_unsettled");
        }
        return;
    }
    if (!coop_roster_ready_window_elapsed(
            group,
            party_count,
            controller_target,
            party_front,
            desired_player_one,
            desired_slot,
            desired_player_two,
            desired_player_two_slot,
            desired_player_three,
            desired_player_three_slot)) {
        return;
    }
    if (controller_target == desired_player_one && desired_slot == 0u) {
        void *controlled_player_two;
        void *controlled_player_three;
        BOOL player_two_requested;
        BOOL player_three_requested;

        if (!SudekiMpSplitScreenRosterLeadReady(
                controller_target,
                party_front,
                desired_player_one,
                controller_mode_80,
                controller_mode_84,
                group_state_d0,
                group_switching_d6,
                group_state_d7,
                next_character_action,
                previous_character_action)) {
            reset_coop_roster_ready_window();
            coop_roster_control_phase = 0;
            if (coop_control_seats_present()) {
                (void)release_coop_control_for_retry(
                    "player_one_lead_not_ready");
            }
            return;
        }
        controlled_player_two =
            SudekiMpControlSeparationSeatCharacter(1u);
        controlled_player_three =
            SudekiMpControlSeparationSeatCharacter(2u);
        player_two_requested =
            SudekiMpControlSeparationSeatRequested(1u);
        player_three_requested =
            SudekiMpControlSeparationSeatRequested(2u);

        /* Fixed 0x07 is one atomic admission contract.  Both external seats
         * must have current hub identities before either native AI override
         * is requested; otherwise a missing P3 pad could leave P2 claimed
         * indefinitely.  Any partial claim unwinds in reverse order. */
        if (fixed_three_assignment &&
            !SudekiMpControlSeparationFixedThreeInputPreflightPolicy(
                SudekiMpControlSeparationSeatInputReady(1u),
                SudekiMpControlSeparationSeatInputReady(2u))) {
            if (player_two_requested || controlled_player_two != NULL ||
                player_three_requested || controlled_player_three != NULL) {
                (void)release_coop_control_for_retry(
                    "fixed_three_input_preflight_not_ready");
            }
            return;
        }

        if (!fixed_three_assignment &&
            (player_three_requested || controlled_player_three != NULL)) {
            (void)release_coop_control_for_retry(
                "p3_present_for_two_seat_assignment");
            return;
        }
        if (!player_two_requested || controlled_player_two == NULL) {
            if (player_three_requested || controlled_player_three != NULL) {
                (void)release_coop_control_for_retry(
                    "p3_present_before_exact_p2");
                return;
            }
            if (SudekiMpControlSeparationRequestSeatCharacter(
                    1u, desired_player_two) &&
                coop_roster_control_phase != 1) {
                coop_roster_control_phase = 1;
                SudekiMpLogFormat(
                    "split_screen_render event=co_op_roster "
                    "phase=claim_player_two player_two=0x%08lx "
                    "policy=exact_selected_identity_before_p3_or_split\r\n",
                    (unsigned long)(uintptr_t)desired_player_two);
            }
            return;
        }
        if (controlled_player_two != desired_player_two) {
            (void)release_coop_control_for_retry("wrong_p2_identity");
            return;
        }
        if (!SudekiMpControlSeparationSeatActive(1u)) {
            return;
        }
        if (fixed_three_assignment &&
            !SudekiMpControlSeparationSeatInputLeaseActive(1u)) {
            (void)release_coop_control_for_retry(
                "fixed_three_p2_input_lease_lost_before_commit");
            return;
        }
        if (!fixed_three_assignment &&
            !SudekiMpControlSeparationSeatInputReady(1u)) {
            return;
        }
        if (fixed_three_assignment) {
            if (!player_three_requested || controlled_player_three == NULL) {
                if (SudekiMpControlSeparationRequestSeatCharacter(
                        2u, desired_player_three) &&
                    coop_roster_control_phase != 4) {
                    coop_roster_control_phase = 4;
                    SudekiMpLogFormat(
                        "split_screen_render event=co_op_roster "
                        "phase=claim_player_three player_three=0x%08lx "
                        "policy=only_after_exact_active_input_ready_p2\r\n",
                        (unsigned long)(uintptr_t)desired_player_three);
                }
                return;
            }
            if (controlled_player_three != desired_player_three) {
                (void)release_coop_control_for_retry(
                    "wrong_p3_identity");
                return;
            }
            if (!SudekiMpControlSeparationSeatActive(2u)) {
                return;
            }
            if (!SudekiMpControlSeparationSeatInputLeaseActive(2u)) {
                (void)release_coop_control_for_retry(
                    "fixed_three_p3_input_lease_lost_before_commit");
                return;
            }
        }
        {
            BOOL control_locked =
                SudekiMpControlSeparationSetRoleLock(TRUE);
            BOOL runtime_enabled = control_locked &&
                SudekiMpSplitScreenSetRuntimeEnabled(TRUE);
            BOOL roles_locked = runtime_enabled &&
                lock_coop_roster_roles(
                    desired_player_one,
                    desired_player_two,
                    desired_player_three,
                    fixed_three_assignment);

            if (!roles_locked) {
                (void)release_coop_roster_runtime(
                    "runtime_or_role_lock_failed");
                SudekiMpLogWrite(
                    "split_screen_render event=co_op_roster "
                    "phase=commit status=rolled_back "
                    "reason=runtime_or_role_lock_failed\r\n");
            } else {
                coop_roster_control_phase = 3;
                SudekiMpLogFormat(
                    "split_screen_render event=co_op_roster phase=applied "
                    "player_one=0x%08lx player_two=0x%08lx "
                    "player_three=0x%08lx active_mask=0x%02lx "
                    "player_one_slot=%u player_two_slot=%u "
                    "player_three_slot=%u "
                    "policy=native_p1_rotation_then_p2_then_p3_exact_claim_atomic_role_lock\r\n",
                    (unsigned long)(uintptr_t)desired_player_one,
                    (unsigned long)(uintptr_t)desired_player_two,
                    (unsigned long)(uintptr_t)desired_player_three,
                    (unsigned long)assignment.active_human_mask,
                    desired_slot,
                    desired_player_two_slot,
                    desired_player_three_slot
                );
            }
        }
        return;
    }
    if (coop_control_seats_present()) {
        (void)release_coop_control_for_retry(
            "native_p1_rotation_requires_no_companion_leases");
        return;
    }
    if (controller_target != party_front ||
        !readable_memory(
            game_base + RVA_CHARACTER_SWITCH_UI_GATE_GLOBAL,
            sizeof(character_switch_ui_gate)) ||
        !readable_memory(
            game_base + RVA_CHARACTER_SWITCH_INPUT_BLOCK_GLOBAL,
            sizeof(void *)) ||
        *(void **)(game_base +
            RVA_CHARACTER_SWITCH_INPUT_BLOCK_GLOBAL) != NULL ||
        !readable_memory(controller_target, 0x94u) ||
        pc_quit_screen_visible() || quick_menu_visible()) {
        reset_coop_roster_ready_window();
        return;
    }
    character_switch_ui_gate = *(uint8_t **)(
        game_base + RVA_CHARACTER_SWITCH_UI_GATE_GLOBAL);
    if (character_switch_ui_gate != NULL &&
        (!readable_memory(
             character_switch_ui_gate,
             CHARACTER_SWITCH_UI_GATE_OFFSET + sizeof(uint8_t)) ||
         character_switch_ui_gate[CHARACTER_SWITCH_UI_GATE_OFFSET] != 0u)) {
        reset_coop_roster_ready_window();
        return;
    }
    character_switch_actor = *(uint8_t **)(
        (uint8_t *)controller_target + 0x90u);
    if (!readable_memory(
            character_switch_actor,
            CHARACTER_SWITCH_FLAGS_60_OFFSET + sizeof(uint32_t))) {
        reset_coop_roster_ready_window();
        return;
    }
    actor_flags_50 = *(uint32_t *)(
        character_switch_actor + CHARACTER_SWITCH_FLAGS_50_OFFSET);
    actor_flags_60 = *(uint32_t *)(
        character_switch_actor + CHARACTER_SWITCH_FLAGS_60_OFFSET);
    if ((actor_flags_50 & CHARACTER_SWITCH_BUSY_FLAGS_50) != 0u ||
        (actor_flags_60 & CHARACTER_SWITCH_BUSY_FLAGS_60) != 0u) {
        reset_coop_roster_ready_window();
        return;
    }
    {
        BOOL use_previous = desired_slot > (unsigned int)party_count / 2u;
        void *consumer = game_base + (use_previous ?
            RVA_GROUP_PLAYERS_PREVIOUS_CHARACTER :
            RVA_GROUP_PLAYERS_NEXT_CHARACTER);
        void *before_target = controller_target;
        void *before_front = party_front;
        void *after_target;
        void *after_front;
        unsigned int attempt = coop_roster_rotation_attempts + 1u;
        BOOL target_changed;
        BOOL front_changed;
        BOOL coherent_change;
        const char *status;

        call_group_players_character_switch(group, consumer);
        after_target = *(void **)(controller + CONTROLLER_TARGET_OFFSET);
        after_front = *(void **)(group + PARTY_SLOT_ZERO_OFFSET);
        target_changed = after_target != before_target;
        front_changed = after_front != before_front;
        coherent_change = target_changed && front_changed &&
            after_target == after_front;
        status = coherent_change ? "accepted" :
            ((target_changed || front_changed) ?
                "invariant_rejected" : "deferred");
        reset_coop_roster_ready_window();
        if (coherent_change) {
            coop_roster_rotation_attempts = 0u;
        } else {
            coop_roster_rotation_attempts = attempt;
        }
        coop_roster_rotation_previous = use_previous;
        SudekiMpLogFormat(
            "split_screen_render event=co_op_roster phase=rotate "
            "status=%s direction=%s desired_type=0x%02lx "
            "desired_slot=%u attempt=%u before_target=0x%08lx "
            "after_target=0x%08lx before_front=0x%08lx "
            "after_front=0x%08lx "
            "policy=exact_native_switch_consumer_game_thread_verified\r\n",
            status,
            use_previous ? "previous" : "next",
            (unsigned long)assignment.actor_type_by_seat[0],
            desired_slot,
            attempt,
            (unsigned long)(uintptr_t)before_target,
            (unsigned long)(uintptr_t)after_target,
            (unsigned long)(uintptr_t)before_front,
            (unsigned long)(uintptr_t)after_front
        );
    }
}

void SudekiMpSplitScreenSetOverlayRenderer(
    SudekiMpSplitScreenOverlayRenderer renderer
) {
    overlay_renderer = renderer;
}

void SudekiMpSplitScreenSetModOwnedBlacksmithActiveQuery(
    SudekiMpModOwnedBlacksmithActiveQuery query
) {
    mod_owned_blacksmith_active_query = query;
}

BOOL SudekiMpInstallSplitScreenRender(
    HMODULE game_module,
    BOOL enable_second_player_camera,
    BOOL enable_dual_camera_frame_cache,
    UINT toggle_second_player_camera_virtual_key,
    BOOL enable_skill_camera_routing,
    BOOL enable_second_player_controller_camera,
    BOOL enable_native_second_player_camera_collision,
    BOOL enable_split_screen_ranged_model_isolation,
    BOOL enable_spirit_strike_viewport_effect_isolation,
    float controller_camera_deadzone,
    float controller_camera_yaw_speed,
    float controller_camera_pitch_speed,
    float controller_camera_maximum_pitch
) {
    uint8_t *base;
    void *resolved_group_players_get_player_group = NULL;

    if (game_module == NULL || original_render_start != NULL ||
        original_frame_end != NULL ||
        (enable_dual_camera_frame_cache &&
         !enable_second_player_camera) ||
        (enable_skill_camera_routing &&
         (!enable_second_player_camera ||
          !enable_dual_camera_frame_cache)) ||
        (enable_split_screen_ranged_model_isolation &&
         (!enable_second_player_camera ||
          !enable_dual_camera_frame_cache)) ||
        (enable_spirit_strike_viewport_effect_isolation &&
         (!enable_second_player_camera ||
          !enable_dual_camera_frame_cache)) ||
        (enable_second_player_controller_camera &&
         (!enable_second_player_camera ||
          !enable_dual_camera_frame_cache ||
          (SudekiMpInputBridgeIdentity() == NULL &&
           SudekiMpLocalInputHubRequestedMask() !=
                FIXED_THREE_HUMAN_MASK) ||
          controller_camera_deadzone < 0.0f ||
          controller_camera_deadzone >= 0.95f ||
          controller_camera_yaw_speed <= 0.0f ||
          controller_camera_yaw_speed > 10.0f ||
          controller_camera_pitch_speed <= 0.0f ||
          controller_camera_pitch_speed > 10.0f ||
          controller_camera_maximum_pitch <= 0.0f ||
          controller_camera_maximum_pitch > 1.2f)) ||
        (enable_native_second_player_camera_collision &&
         (!enable_second_player_camera ||
          !enable_dual_camera_frame_cache)) ||
        (enable_second_player_camera &&
         (toggle_second_player_camera_virtual_key == 0u ||
          toggle_second_player_camera_virtual_key > 0xffu))) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    base = (uint8_t *)game_module;
    if (enable_native_second_player_camera_collision &&
        !native_camera_wrapper_signatures_match(
            game_module,
            base,
            &resolved_group_players_get_player_group)) {
        SetLastError(ERROR_INVALID_DATA);
        return FALSE;
    }
    if (!pc_quit_screen_show_signature_matches(base) ||
        !quick_menu_is_active_signature_matches(base) ||
        !shared_interaction_modal_signatures_match(base) ||
        !character_switch_consumer_signatures_match(base)) {
        SudekiMpLogFormat(
            "split_screen_render_preflight common_quit=%u common_quick=%u "
            "shared_interaction_modal=%u character_switch_consumer=%u\r\n",
            pc_quit_screen_show_signature_matches(base),
            quick_menu_is_active_signature_matches(base),
            shared_interaction_modal_signatures_match(base),
            character_switch_consumer_signatures_match(base));
    }
    if (enable_dual_camera_frame_cache) {
        SudekiMpLogFormat(
            "split_screen_render_preflight portrait_selector=%u quick_submit=%u quick_owner=%u\r\n",
            portrait_selector_signatures_match(base),
            memcmp(
                base + RVA_QUICK_MENU_RENDER_SUBMIT,
                expected_quick_menu_render_submit_entry,
                sizeof(expected_quick_menu_render_submit_entry)
            ) == 0,
            quick_menu_owner_signatures_match(base));
    }
    if (enable_second_player_camera) {
        SudekiMpLogFormat(
            "split_screen_render_preflight camera_add=%u camera_remove=%u "
            "camera_get=%u camera_render=%u native_target=%u "
            "native_mode=%u native_state=%u native_input=%u "
            "native_input_slot=%u native_combat_gate=%u "
            "native_target_wrapper=%u\r\n",
            memcmp(
                base + RVA_CAMERA_MANAGER_ADD_CAMERA,
                expected_camera_manager_add_camera_entry,
                sizeof(expected_camera_manager_add_camera_entry)
            ) == 0,
            memcmp(
                base + RVA_CAMERA_MANAGER_REMOVE_CAMERA,
                expected_camera_manager_remove_camera_entry,
                sizeof(expected_camera_manager_remove_camera_entry)
            ) == 0,
            memcmp(
                base + RVA_CAMERA_MANAGER_GET_CAMERA,
                expected_camera_manager_get_camera_entry,
                sizeof(expected_camera_manager_get_camera_entry)
            ) == 0,
            !enable_skill_camera_routing || memcmp(
                base + RVA_CAMERA_MANAGER_SET_RENDER_CAMERA,
                expected_camera_manager_set_render_camera_entry,
                sizeof(expected_camera_manager_set_render_camera_entry)
            ) == 0,
            !enable_native_second_player_camera_collision || memcmp(
                base + RVA_CAMERA_MANAGER_SET_CAMERA_TARGET,
                expected_camera_manager_set_camera_target_entry,
                sizeof(expected_camera_manager_set_camera_target_entry)
            ) == 0,
            !enable_native_second_player_camera_collision || memcmp(
                base + RVA_CAMERA_MANAGER_GET_CAMERA_MODE,
                expected_camera_manager_get_camera_mode_entry,
                sizeof(expected_camera_manager_get_camera_mode_entry)
            ) == 0,
            !enable_native_second_player_camera_collision || memcmp(
                base + RVA_CAMERA_MANAGER_SET_CAMERA_STATE,
                expected_camera_manager_set_camera_state_entry,
                sizeof(expected_camera_manager_set_camera_state_entry)
            ) == 0,
            (!enable_second_player_controller_camera &&
             !enable_native_second_player_camera_collision) || memcmp(
                base + RVA_CAMERA_INPUT_EVENT,
                expected_camera_input_event_entry,
                sizeof(expected_camera_input_event_entry)
            ) == 0,
            (!enable_second_player_controller_camera &&
             !enable_native_second_player_camera_collision) ||
                *(void **)(base + RVA_CAMERA_INPUT_EVENT_VTABLE_SLOT) ==
                    base + RVA_CAMERA_INPUT_EVENT,
            !enable_native_second_player_camera_collision || memcmp(
                base + RVA_GROUP_PLAYERS_IN_COMBAT,
                expected_group_players_in_combat_entry,
                sizeof(expected_group_players_in_combat_entry)
            ) == 0,
            !enable_native_second_player_camera_collision ||
                native_camera_wrapper_signatures_match(
                    game_module, base, NULL));
    }
    if (!pc_quit_screen_show_signature_matches(base) ||
        !quick_menu_is_active_signature_matches(base) ||
        !shared_interaction_modal_signatures_match(base) ||
        !character_switch_consumer_signatures_match(base)) {
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
         ((enable_second_player_controller_camera ||
           enable_native_second_player_camera_collision) &&
          (memcmp(
              base + RVA_CAMERA_INPUT_EVENT,
              expected_camera_input_event_entry,
              sizeof(expected_camera_input_event_entry)) != 0 ||
           *(void **)(base + RVA_CAMERA_INPUT_EVENT_VTABLE_SLOT) !=
              base + RVA_CAMERA_INPUT_EVENT)) ||
         (enable_native_second_player_camera_collision &&
          (memcmp(
              base + RVA_CAMERA_MANAGER_SET_CAMERA_TARGET,
              expected_camera_manager_set_camera_target_entry,
              sizeof(expected_camera_manager_set_camera_target_entry)) != 0 ||
           memcmp(
              base + RVA_CAMERA_MANAGER_GET_CAMERA_MODE,
              expected_camera_manager_get_camera_mode_entry,
              sizeof(expected_camera_manager_get_camera_mode_entry)) != 0 ||
           memcmp(
              base + RVA_CAMERA_MANAGER_SET_CAMERA_STATE,
              expected_camera_manager_set_camera_state_entry,
              sizeof(expected_camera_manager_set_camera_state_entry)) != 0 ||
           memcmp(
              base + RVA_GROUP_PLAYERS_IN_COMBAT,
              expected_group_players_in_combat_entry,
              sizeof(expected_group_players_in_combat_entry)) != 0 ||
           !native_camera_wrapper_signatures_match(
               game_module, base, NULL))) ||
         (enable_skill_camera_routing &&
          (memcmp(
              base + RVA_CAMERA_MANAGER_SET_RENDER_CAMERA,
              expected_camera_manager_set_render_camera_entry,
              sizeof(expected_camera_manager_set_render_camera_entry)) != 0 ||
           memcmp(
              base + RVA_GAME_SPEED_SET_MODE,
              expected_game_speed_set_mode_entry,
              sizeof(expected_game_speed_set_mode_entry)) != 0 ||
           memcmp(
              base + RVA_FIXED_ALTERNATE_SPEED,
              expected_fixed_alternate_speed,
              sizeof(expected_fixed_alternate_speed)) != 0)) ||
         (enable_second_player_controller_camera && memcmp(
            base + RVA_POSITION_SET_FORWARD,
            expected_position_set_forward_entry,
            sizeof(expected_position_set_forward_entry)) != 0) ||
         (enable_split_screen_ranged_model_isolation &&
          (memcmp(
              base + RVA_GROUP_PLAYERS_IN_COMBAT,
              expected_group_players_in_combat_entry,
              sizeof(expected_group_players_in_combat_entry)) != 0 ||
           !animation_renderer_signatures_match(base) ||
           memcmp(
              base + RVA_RANGED_WEAPON_REATTACH,
              expected_ranged_weapon_reattach_entry,
              sizeof(expected_ranged_weapon_reattach_entry)) != 0 ||
           memcmp(
              base + RVA_RENDER_LOCATOR_INDEX,
              expected_render_locator_index_entry,
              sizeof(expected_render_locator_index_entry)) != 0)) ||
         (enable_dual_camera_frame_cache &&
          (!portrait_selector_signatures_match(base) ||
           !quick_menu_owner_signatures_match(base) ||
           memcmp(
              base + RVA_QUICK_MENU_RENDER_SUBMIT,
              expected_quick_menu_render_submit_entry,
              sizeof(expected_quick_menu_render_submit_entry)) != 0)) ||
         (enable_spirit_strike_viewport_effect_isolation &&
          memcmp(
              base + RVA_HISTORY_RESOURCE_FACTORY,
              expected_history_resource_factory_entry,
              sizeof(expected_history_resource_factory_entry)
          ) != 0))) {
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
    original_quick_menu_render_submit =
        (QuickMenuRenderSubmitFunction)(
            game_base + RVA_QUICK_MENU_RENDER_SUBMIT
        );
    quick_menu_is_active = (QuickMenuIsActiveFunction)(
        game_base + RVA_QUICK_MENU_IS_ACTIVE
    );
    quick_menu_close = enable_dual_camera_frame_cache ?
        (QuickMenuCloseFunction)(game_base + RVA_QUICK_MENU_CLOSE) : NULL;
    quick_menu_start = enable_dual_camera_frame_cache ?
        (QuickMenuStartFunction)(game_base + RVA_QUICK_MENU_START) : NULL;
    original_quick_menu_input = enable_dual_camera_frame_cache ?
        (QuickMenuInputFunction)(game_base + RVA_QUICK_MENU_INPUT) : NULL;
    original_quick_menu_native_toggle = enable_dual_camera_frame_cache ?
        game_base + RVA_QUICK_MENU_NATIVE_TOGGLE : NULL;
    quick_menu_party_pointer_cleanup = enable_dual_camera_frame_cache ?
        (TrackedEntityCleanupFunction)(
            game_base + RVA_TRACKED_ENTITY_CLEANUP) : NULL;
    shop_is_active = (NativeModalIsActiveFunction)(
        game_base + RVA_SHOP_IS_ACTIVE
    );
    blacksmith_is_active = (NativeModalIsActiveFunction)(
        game_base + RVA_BLACKSMITH_IS_ACTIVE
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
    skill_camera_routing_enabled = enable_skill_camera_routing;
    second_player_controller_camera_enabled =
        enable_second_player_controller_camera;
    native_second_player_camera_collision_enabled =
        enable_native_second_player_camera_collision;
    native_second_player_camera_collision_session_enabled =
        enable_native_second_player_camera_collision;
    player_two_native_camera_recreate_requested = FALSE;
    split_render_frame_sequence = 0u;
    split_screen_ranged_model_isolation_enabled =
        enable_split_screen_ranged_model_isolation;
    spirit_strike_viewport_effect_isolation_enabled =
        enable_spirit_strike_viewport_effect_isolation;
    original_motion_blur_post_render = (MotionBlurPostRenderFunction)(
        game_base + RVA_MOTION_BLUR_POST_RENDER
    );
    original_screenshot_post_render = (ScreenshotPostRenderFunction)(
        game_base + RVA_SCREENSHOT_POST_RENDER
    );
    native_history_resource_factory =
        (NativeHistoryResourceFactoryFunction)(
            game_base + RVA_HISTORY_RESOURCE_FACTORY
        );
    spirit_player_two_history_wrapper = NULL;
    spirit_player_two_history_surface = NULL;
    spirit_player_two_history_width = 0u;
    spirit_player_two_history_height = 0u;
    spirit_history_resource_logged = FALSE;
    second_player_controller_camera_deadzone = controller_camera_deadzone;
    second_player_controller_camera_yaw_speed = controller_camera_yaw_speed;
    second_player_controller_camera_pitch_speed = controller_camera_pitch_speed;
    second_player_controller_camera_maximum_pitch =
        controller_camera_maximum_pitch;
    reset_player_two_controller_camera();
    pending_skill_camera_caster = NULL;
    ZeroMemory(player_skill_cameras, sizeof(player_skill_cameras));
    ZeroMemory(player_skill_render_states, sizeof(player_skill_render_states));
    skill_camera_request_sequence = 0u;
    skill_speed_request_sequence = 0u;
    skill_time_scale_override_active = FALSE;
    skill_camera_trace_sequence = 0u;
    skill_camera_trace_frame = 0u;
    skill_camera_trace_started_tick = 0u;
    skill_camera_trace_active = FALSE;
    skill_camera_history_logged_callbacks = 0u;
    skill_camera_history_tail_until = 0u;
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
    camera_manager_set_camera_target =
        enable_native_second_player_camera_collision ?
            (CameraManagerSetCameraTargetFunction)(
                game_base + RVA_CAMERA_MANAGER_SET_CAMERA_TARGET) : NULL;
    group_players_get_player_group =
        enable_native_second_player_camera_collision ?
            (GroupPlayersGetPlayerGroupFunction)
                resolved_group_players_get_player_group : NULL;
    gel_pointer_resolve_entity =
        enable_native_second_player_camera_collision ?
            (GelPointerResolveEntityFunction)(
                game_base + RVA_GEL_POINTER_RESOLVE_ENTITY) : NULL;
    tracked_entity_cleanup =
        enable_native_second_player_camera_collision ?
            (TrackedEntityCleanupFunction)(
                game_base + RVA_TRACKED_ENTITY_CLEANUP) : NULL;
    camera_manager_get_camera_mode =
        enable_native_second_player_camera_collision ?
            (CameraManagerGetCameraModeFunction)(
                game_base + RVA_CAMERA_MANAGER_GET_CAMERA_MODE) : NULL;
    camera_manager_set_camera_state =
        enable_native_second_player_camera_collision ?
            (CameraManagerSetCameraStateFunction)(
                game_base + RVA_CAMERA_MANAGER_SET_CAMERA_STATE) : NULL;
    original_camera_input_event =
        (enable_second_player_controller_camera ||
         enable_native_second_player_camera_collision) ?
            (CameraInputEventFunction)(
                game_base + RVA_CAMERA_INPUT_EVENT) : NULL;
    group_players_in_combat =
        (enable_split_screen_ranged_model_isolation ||
         enable_native_second_player_camera_collision) ?
        (GroupPlayersInCombatFunction)(
            game_base + RVA_GROUP_PLAYERS_IN_COMBAT
        ) : NULL;
    ranged_weapon_reattach_function =
        enable_split_screen_ranged_model_isolation ?
            game_base + RVA_RANGED_WEAPON_REATTACH : NULL;
    render_locator_index_function =
        enable_split_screen_ranged_model_isolation ?
            game_base + RVA_RENDER_LOCATOR_INDEX : NULL;
    position_set_forward_function = game_base + RVA_POSITION_SET_FORWARD;
    player_two_view_requested = FALSE;
    rendered_player_two_this_frame = FALSE;
    viewport_hud_binding_active = FALSE;
    shared_interaction_modal_observation =
        SUDEKIMP_SHARED_INTERACTION_MODAL_NONE;
    InterlockedExchange(&native_save_modal_opening, 0);
    InterlockedExchange(&native_movie_gate_depth, 0);
    shared_interaction_modal_recovery_pending = FALSE;
    shared_interaction_modal_had_live_split = FALSE;
    shared_interaction_modal_published_kind = SUDEKIMP_INTERACTION_NONE;
    genuine_quick_menu_active_this_frame = FALSE;
    quick_menu_live_player_two_available_this_frame = FALSE;
    quick_menu_render_submit_isolation_logged = FALSE;
    quick_menu_isolation_active = FALSE;
    quick_menu_isolation_tail_active = FALSE;
    quick_menu_genuine_visible_previous_frame =
        genuine_quick_menu_visible();
    quick_menu_isolation_failed_for_open_menu =
        quick_menu_genuine_visible_previous_frame;
    quick_menu_expected_player_two = FALSE;
    quick_menu_render_phase_confirmed_this_frame = FALSE;
    quick_menu_submit_seen_since_frame_end = FALSE;
    render_only_swap_active = FALSE;
    render_only_camera_slot = NULL;
    render_only_applied_state = NULL;
    render_only_original_state = NULL;
    player_two_hud_ownership_logged = FALSE;
    viewport_portrait_ownership_logged = FALSE;
    ranged_model_isolation_logged = FALSE;
    ranged_first_person_model_isolation_logged = FALSE;
    ranged_model_animation_mirror_logged = FALSE;
    ranged_first_person_animation_mirror_logged = FALSE;
    ranged_player_two_first_person_rejection_logged = FALSE;
    ranged_weapon_stage_logged = FALSE;
    ranged_weapon_restore_logged = FALSE;
    ranged_weapon_reattach_failure_logged = FALSE;
    ranged_locator_trace_character = NULL;
    ranged_locator_trace_pitch_bucket = -1;
    ranged_locator_trace_last_tick = 0u;
    ranged_locator_trace_sequence = 0u;
    ranged_state_details_inventory_complete = FALSE;
    ranged_state_details_inventory_pending_logged = FALSE;
    ZeroMemory(
        &ranged_world_compositor_state,
        sizeof(ranged_world_compositor_state)
    );
    ranged_transform_trace_last_tick = 0u;
    ranged_transform_trace_sequence = 0u;
    reset_ranged_animation_trace();
    reset_hud_mapping_trace();
    reset_minimap_owner_schedule();
    player_two_facing_logged = FALSE;
    ZeroMemory(
        ranged_model_render_swaps,
        sizeof(ranged_model_render_swaps)
    );
    quick_menu_gate_last_state = -1;
    quick_menu_next_serial = 0u;
    reset_quick_menu_owner_session();
    spirit_presentation_last_state = -1;
    spirit_presentation_logged_views = 0u;
    spirit_player_two_presentation_last_trace_tick = 0u;
    spirit_player_two_presentation_position_valid = FALSE;
    ZeroMemory(
        spirit_player_two_presentation_position,
        sizeof(spirit_player_two_presentation_position)
    );
    spirit_effect_isolation_logged = FALSE;
    spirit_capture_completion_logged = FALSE;
    runtime_split_enabled = TRUE;
    runtime_authorized_at_render_start = FALSE;
    coop_role_lock_active = FALSE;
    coop_locked_player_one = NULL;
    coop_locked_player_two = NULL;
    coop_locked_player_three = NULL;
    ZeroMemory(&fixed_three_runtime, sizeof(fixed_three_runtime));
    if (!coop_roster_valid) {
        SudekiMpCoopRosterAssignmentStoreInitialize(
            &coop_roster_assignment_store);
    }
    coop_roster_participation_requested = TRUE;
    coop_roster_party_transition_active = FALSE;
    coop_roster_runtime_release_pending = FALSE;
    coop_roster_rotation_attempts = 0u;
    coop_roster_rotation_previous = FALSE;
    coop_roster_last_presence_mask = ~0u;
    reset_coop_roster_ready_window();
    coop_roster_control_phase = -1;
    overlay_renderer = NULL;
    mod_owned_blacksmith_active_query = NULL;
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
        ((second_player_controller_camera_enabled ||
          native_second_player_camera_collision_enabled) &&
         !SudekiMpInstallPointerHook(
              &camera_input_event_hook,
              (void **)(game_base +
                  RVA_CAMERA_INPUT_EVENT_VTABLE_SLOT),
              original_camera_input_event,
              route_camera_input_event)) ||
        (dual_camera_frame_cache_enabled &&
         (!SudekiMpInstallPointerHook(
              &quick_menu_render_submit_hook,
              (void **)(game_base +
                  RVA_QUICK_MENU_RENDER_SUBMIT_VTABLE_SLOT),
              original_quick_menu_render_submit,
              route_quick_menu_render_submit) ||
          !install_quick_menu_owner_hooks() ||
          !SudekiMpInstallRelativeCallHook(
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
              split_screen_hud_party_pointer_copy_entry) ||
          !SudekiMpInstallRelativeCallHook(
              &minimap_update_pointer_hook,
              game_base + RVA_MINIMAP_UPDATE_POINTER_CALL,
              original_hud_party_pointer_copy,
              split_screen_minimap_scheduled_party_pointer_copy_entry) ||
          !SudekiMpInstallRelativeCallHook(
              &minimap_render_pointer_hook,
              game_base + RVA_MINIMAP_RENDER_POINTER_CALL,
              original_hud_party_pointer_copy,
              split_screen_minimap_render_party_pointer_copy_entry))) ||
        (skill_camera_routing_enabled &&
         (!SudekiMpInstallInlineHook(
              &set_render_camera_hook,
              game_base + RVA_CAMERA_MANAGER_SET_RENDER_CAMERA,
              expected_camera_manager_set_render_camera_entry,
              sizeof(expected_camera_manager_set_render_camera_entry),
              route_skill_render_camera) ||
          !SudekiMpInstallInlineHook(
              &set_game_speed_mode_hook,
              game_base + RVA_GAME_SPEED_SET_MODE,
              expected_game_speed_set_mode_entry,
              sizeof(expected_game_speed_set_mode_entry),
              route_game_speed_set_mode))) ||
        (spirit_strike_viewport_effect_isolation_enabled &&
         (!SudekiMpInstallPointerHook(
              &motion_blur_post_render_hook,
              (void **)(game_base +
                  RVA_MOTION_BLUR_POST_RENDER_VTABLE_SLOT),
              original_motion_blur_post_render,
              route_motion_blur_post_render) ||
          !SudekiMpInstallPointerHook(
              &screenshot_post_render_hook,
              (void **)(game_base +
                  RVA_SCREENSHOT_POST_RENDER_VTABLE_SLOT),
              original_screenshot_post_render,
              route_screenshot_post_render)))) {
        SudekiMpUninstallSplitScreenRender();
        return FALSE;
    }
    split_screen_render_installed = TRUE;
    SudekiMpLogFormat(
        "split_screen_render event=install render_start_rva=0x%08lx render_start_callsite_rva=0x%08lx frame_end_rva=0x%08lx frame_end_callsite_rva=0x%08lx quit_render_rva=0x%08lx quit_render_callsite_rva=0x%08lx quick_menu_render_submit_rva=0x%08lx quick_menu_render_submit_vtable_slot_rva=0x%08lx scope=render_only_camera_swap_plus_post_end_scene_compositor gameplay_state_gated shared_menu_gate=pc_quit_screen_plus_0x1c2 quick_menu_gate=owner_pinned_native_menu_cache_plus_preserved_non_owner_clean_cache shared_menu_backdrop=frozen_cached_camera_pair_before_native_quit_ui layout=left_right camera_policy=%s second_player_named_camera=%s dual_camera_frame_cache=%s viewport_hud_party_slot_swap=%s viewport_hud_portrait_assignment=%s viewport_minimap_ownership=%s skill_camera_routing=%s skill_world_time=%s controller_camera=%s native_player_two_camera_collision=%s ranged_model_isolation=%s spirit_effect_isolation=%s toggle_virtual_key=0x%02lx\r\n",
        (unsigned long)RVA_RENDER_START,
        (unsigned long)RVA_RENDER_START_CALL,
        (unsigned long)RVA_FRAME_END,
        (unsigned long)RVA_FRAME_END_CALL,
        (unsigned long)RVA_PC_QUIT_SCREEN_RENDER,
        (unsigned long)RVA_PC_QUIT_SCREEN_RENDER_CALL,
        (unsigned long)RVA_QUICK_MENU_RENDER_SUBMIT,
        (unsigned long)RVA_QUICK_MENU_RENDER_SUBMIT_VTABLE_SLOT,
        dual_camera_frame_cache_enabled ?
            "alternating_clean_frame_cache" :
            "duplicate_finished_native_frame",
        second_player_camera_enabled ? "true" : "false",
        dual_camera_frame_cache_enabled ? "true" : "false",
        dual_camera_frame_cache_enabled ? "true" : "false",
        dual_camera_frame_cache_enabled ?
            "skip_redundant_same_owner_portrait_assignment" :
            "disabled",
        dual_camera_frame_cache_enabled ?
            "exact_update_owner_latched_through_render_native_last_cluster_snapshot_unchanged" :
            "disabled",
        skill_camera_routing_enabled ?
            "caster_viewport_only" : "disabled",
        skill_camera_routing_enabled ?
            "native_mode_handshake_with_shared_realtime_scale" : "disabled",
        second_player_controller_camera_enabled ?
            "right_stick_render_only_player_two" : "disabled",
        native_second_player_camera_collision_enabled ?
            "named_exploration_state_targeted_via_one_shot_engine_ptrobj_owned_gel_group_ptr_wrapper_no_mod_destructor_no_reuse_generation_scoped_input_broadcast_suppressed_no_independent_p2_right_stick_native_ready" :
            "disabled",
        split_screen_ranged_model_isolation_enabled ?
            "p1_observer_world_body_native_weapon_rebind_state_details_inventory_root_pitch_rejected_disabled_p2_owner_stable_third_person_fallback" :
            "disabled",
        spirit_strike_viewport_effect_isolation_enabled ?
            "native_history_resource_per_viewport_callbacks_unmodified" :
            "disabled",
        (unsigned long)second_player_camera_virtual_key
    );
    if (split_screen_ranged_model_isolation_enabled) {
        SudekiMpLogFormat(
            "split_screen_render event=ranged_observer_pitch phase=disabled reason=rejected_whole_root_feet_pivot policy=preserve_stable_observer_animation_mirror_while_reading_candidate_state_details\r\n"
        );
        SudekiMpLogFormat(
            "split_screen_render event=ranged_state_details_inventory phase=armed animation_ids=0x97,0x98,0x99 renderer_vtable_rva=0x%08lx renderer_lookup_rva=0x%08lx policy=one_time_direct_component_table_read_exact_fp_and_world_handle_resolution_no_state_lookup_selector_playback_or_animation_write\r\n",
            (unsigned long)RVA_ANIMATION_RENDERER_VTABLE,
            (unsigned long)RVA_ANIMATION_RENDERER_LOOKUP
        );
    }
    if (coop_roster_valid) {
        SudekiMpLogFormat(
            "split_screen_render event=co_op_roster phase=install_preserved "
            "player_one_type=0x%02lx player_two_type=0x%02lx "
            "policy=preinstalled_title_roster_contract\r\n",
            (unsigned long)coop_roster_player_one_type,
            (unsigned long)coop_roster_player_two_type
        );
    }
    return TRUE;
}

void SudekiMpUninstallSplitScreenRender(void) {
    unsigned int quick_menu_hook_index;
    BOOL fixed_three_released;

    /* Named cameras and their full-frame caches are mod-owned. Release them
     * while the exact manager APIs and scene ownership proof are still live,
     * always in the P3-then-P2 order enforced by the helper. */
    fixed_three_released = fixed_three_release_cameras("module_uninstall");
    if (!fixed_three_released) {
        fixed_three_release_frame_surfaces();
        SudekiMpLogWrite(
            "split_screen_render event=fixed_three_seat_camera "
            "phase=release status=quarantined reason=ownership_mismatch "
            "policy=do_not_remove_unproven_native_camera\r\n");
    }
    ZeroMemory(&fixed_three_runtime, sizeof(fixed_three_runtime));
    split_screen_render_installed = FALSE;
    /* Keep a fail-closed authorization state until both render hooks are
     * restored.  Clearing the optional query first would make the default
     * policy permissive while a prefetched wrapper can still dispatch. */
    runtime_split_enabled = FALSE;
    runtime_authorized_at_render_start = FALSE;
    InterlockedExchange(&native_save_modal_opening, 0);
    InterlockedExchange(&native_movie_gate_depth, 0);
    if (shared_interaction_modal_published_kind !=
            SUDEKIMP_INTERACTION_NONE) {
        SudekiMpPlayerStatehoodObserveNativeModal(
            SudekiMpPlayerStatehoodRuntime(),
            SUDEKIMP_INTERACTION_NONE,
            0u,
            GetTickCount()
        );
    }
    if (skill_time_scale_override_active) {
        set_skill_time_scale_override(FALSE);
    }
    restore_player_two_collision_self_cull();
    restore_ranged_model_render_view();
    reset_ranged_world_compositor("module_uninstall");
    restore_render_only_camera();
    SudekiMpRestorePointerHook(&camera_input_event_hook);
    SudekiMpRestorePointerHook(&screenshot_post_render_hook);
    SudekiMpRestorePointerHook(&motion_blur_post_render_hook);
    SudekiMpRestorePointerHook(&quick_menu_render_submit_hook);
    SudekiMpRestoreRelativeCallHook(
        &quick_menu_owner_default_recipient_hook);
    for (quick_menu_hook_index = QUICK_MENU_OWNER_COPY_HOOK_COUNT;
         quick_menu_hook_index > 0u;
         --quick_menu_hook_index) {
        SudekiMpRestoreRelativeCallHook(
            &quick_menu_owner_copy_hooks[quick_menu_hook_index - 1u]);
    }
    SudekiMpRestoreRelativeCallHook(&quick_menu_native_toggle_hook);
    SudekiMpRestorePointerHook(&quick_menu_input_hook);
    SudekiMpRestoreInlineHook(&set_game_speed_mode_hook);
    SudekiMpRestoreInlineHook(&set_render_camera_hook);
    SudekiMpRestoreRelativeCallHook(&minimap_render_pointer_hook);
    SudekiMpRestoreRelativeCallHook(&minimap_update_pointer_hook);
    SudekiMpRestoreRelativeCallHook(&hud_gizmo_status_pointer_hook);
    SudekiMpRestoreRelativeCallHook(&hud_gizmo_name_pointer_hook);
    SudekiMpRestoreRelativeCallHook(&hud_gizmo_values_pointer_hook);
    SudekiMpRestoreRelativeCallHook(&hud_group_values_pointer_hook);
    SudekiMpRestoreRelativeCallHook(&quit_screen_render_hook);
    SudekiMpRestoreRelativeCallHook(&render_start_hook);
    SudekiMpRestoreRelativeCallHook(&frame_end_hook);
    runtime_authorization_query = NULL;
    runtime_authorization_last_state = -1;
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
    ranged_model_isolation_logged = FALSE;
    ranged_first_person_model_isolation_logged = FALSE;
    ranged_model_animation_mirror_logged = FALSE;
    ranged_first_person_animation_mirror_logged = FALSE;
    ranged_player_two_first_person_rejection_logged = FALSE;
    ranged_weapon_stage_logged = FALSE;
    ranged_weapon_restore_logged = FALSE;
    ranged_weapon_reattach_failure_logged = FALSE;
    ranged_locator_trace_character = NULL;
    ranged_locator_trace_pitch_bucket = -1;
    ranged_locator_trace_last_tick = 0u;
    ranged_locator_trace_sequence = 0u;
    ranged_state_details_inventory_complete = FALSE;
    ranged_state_details_inventory_pending_logged = FALSE;
    ranged_transform_trace_last_tick = 0u;
    ranged_transform_trace_sequence = 0u;
    reset_ranged_animation_trace();
    reset_hud_mapping_trace();
    reset_minimap_owner_schedule();
    player_two_facing_logged = FALSE;
    ZeroMemory(
        ranged_model_render_swaps,
        sizeof(ranged_model_render_swaps)
    );
    gameplay_gate_last_state = -1;
    shared_menu_gate_last_state = -1;
    shared_interaction_modal_observation =
        SUDEKIMP_SHARED_INTERACTION_MODAL_NONE;
    shared_interaction_modal_recovery_pending = FALSE;
    shared_interaction_modal_had_live_split = FALSE;
    shared_interaction_modal_published_kind = SUDEKIMP_INTERACTION_NONE;
    quick_menu_gate_last_state = -1;
    spirit_presentation_last_state = -1;
    spirit_presentation_logged_views = 0u;
    spirit_player_two_presentation_last_trace_tick = 0u;
    spirit_player_two_presentation_position_valid = FALSE;
    ZeroMemory(
        spirit_player_two_presentation_position,
        sizeof(spirit_player_two_presentation_position)
    );
    second_player_camera_enabled = FALSE;
    dual_camera_frame_cache_enabled = FALSE;
    second_player_controller_camera_enabled = FALSE;
    native_second_player_camera_collision_enabled = FALSE;
    native_second_player_camera_collision_session_enabled = FALSE;
    player_two_native_camera_recreate_requested = FALSE;
    split_render_frame_sequence = 0u;
    split_screen_ranged_model_isolation_enabled = FALSE;
    spirit_strike_viewport_effect_isolation_enabled = FALSE;
    original_motion_blur_post_render = NULL;
    original_screenshot_post_render = NULL;
    native_history_resource_factory = NULL;
    spirit_player_two_history_wrapper = NULL;
    spirit_player_two_history_surface = NULL;
    spirit_player_two_history_width = 0u;
    spirit_player_two_history_height = 0u;
    spirit_history_resource_logged = FALSE;
    spirit_effect_isolation_logged = FALSE;
    spirit_capture_completion_logged = FALSE;
    second_player_controller_camera_deadzone = 0.0f;
    second_player_controller_camera_yaw_speed = 0.0f;
    second_player_controller_camera_pitch_speed = 0.0f;
    second_player_controller_camera_maximum_pitch = 0.0f;
    reset_player_two_controller_camera();
    skill_camera_routing_enabled = FALSE;
    pending_skill_camera_caster = NULL;
    ZeroMemory(player_skill_cameras, sizeof(player_skill_cameras));
    ZeroMemory(player_skill_render_states, sizeof(player_skill_render_states));
    skill_camera_request_sequence = 0u;
    skill_speed_request_sequence = 0u;
    skill_time_scale_override_active = FALSE;
    skill_camera_trace_sequence = 0u;
    skill_camera_trace_frame = 0u;
    skill_camera_trace_started_tick = 0u;
    skill_camera_trace_active = FALSE;
    skill_camera_history_logged_callbacks = 0u;
    skill_camera_history_tail_until = 0u;
    second_player_camera_virtual_key = 0u;
    second_player_camera_key_was_down = FALSE;
    camera_manager_add_camera = NULL;
    camera_manager_remove_camera = NULL;
    camera_manager_get_camera = NULL;
    camera_manager_set_camera_target = NULL;
    group_players_get_player_group = NULL;
    gel_pointer_resolve_entity = NULL;
    tracked_entity_cleanup = NULL;
    camera_manager_get_camera_mode = NULL;
    camera_manager_set_camera_state = NULL;
    original_camera_input_event = NULL;
    group_players_in_combat = NULL;
    ranged_weapon_reattach_function = NULL;
    render_locator_index_function = NULL;
    position_set_forward_function = NULL;
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
    genuine_quick_menu_active_this_frame = FALSE;
    quick_menu_live_player_two_available_this_frame = FALSE;
    quick_menu_render_submit_isolation_logged = FALSE;
    reset_quick_menu_owner_session();
    quick_menu_isolation_active = FALSE;
    quick_menu_isolation_tail_active = FALSE;
    quick_menu_isolation_failed_for_open_menu = FALSE;
    quick_menu_genuine_visible_previous_frame = FALSE;
    quick_menu_expected_player_two = FALSE;
    quick_menu_render_phase_confirmed_this_frame = FALSE;
    quick_menu_submit_seen_since_frame_end = FALSE;
    render_only_swap_active = FALSE;
    render_only_camera_slot = NULL;
    render_only_applied_state = NULL;
    render_only_original_state = NULL;
    second_player_camera_last_rejection = 0u;
    d3d_device_global = NULL;
    original_render_start = NULL;
    original_frame_end = NULL;
    original_quit_screen_render = NULL;
    original_quick_menu_render_submit = NULL;
    quick_menu_is_active = NULL;
    quick_menu_close = NULL;
    quick_menu_start = NULL;
    original_quick_menu_input = NULL;
    original_quick_menu_native_toggle = NULL;
    quick_menu_party_pointer_cleanup = NULL;
    shop_is_active = NULL;
    blacksmith_is_active = NULL;
    original_hud_party_pointer_copy = NULL;
    original_character_type_to_portrait_enum = NULL;
    original_hud_portrait_resource_select = NULL;
    game_base = NULL;
    runtime_split_enabled = TRUE;
    runtime_authorized_at_render_start = FALSE;
    coop_role_lock_active = FALSE;
    coop_locked_player_one = NULL;
    coop_locked_player_two = NULL;
    coop_locked_player_three = NULL;
    coop_roster_valid = FALSE;
    coop_roster_player_one_type = 0u;
    coop_roster_player_two_type = 0u;
    SudekiMpCoopRosterAssignmentStoreInitialize(
        &coop_roster_assignment_store);
    coop_roster_participation_requested = TRUE;
    coop_roster_party_transition_active = FALSE;
    coop_roster_runtime_release_pending = FALSE;
    coop_roster_last_presence_mask = ~0u;
    reset_coop_roster_ready_window();
    coop_roster_control_phase = -1;
    overlay_renderer = NULL;
    mod_owned_blacksmith_active_query = NULL;
}
