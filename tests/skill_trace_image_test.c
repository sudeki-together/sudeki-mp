#include "engine/skill_activation_abi.h"
#include "engine/player_statehood.h"
#include "hooks/accelerator_cache.h"
#include "hooks/blacksmith_ui_adapter.h"
#include "hooks/skill_trace.h"
#include "hooks/character_switch_trace.h"
#include "hooks/control_separation.h"
#include "hooks/freeroam_camera_input.h"
#include "hooks/player_input_trace.h"
#include "hooks/quick_skill_input.h"
#include "hooks/spirit_strike_input.h"
#include "hooks/split_screen_render.h"
#include "hooks/talos_defense_trace.h"
#include "hooks/zone_transition_trace.h"

#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

enum {
    RVA_USE = 0x000b4810u,
    RVA_USE_CALL = 0x000998a1u,
    RVA_DIRECT_USE_CALL = 0x00027cb1u,
    RVA_QUICK_SKILL_ACTION = 0x00027bf0u,
    RVA_QUICK_SKILL_ACTION_CALL = 0x00027acfu,
    RVA_SKILL_VALIDATE = 0x000b4bc0u,
    RVA_QUICK_SKILL_VALIDATE_CALL = 0x00027c8cu,
    RVA_QUICK_MENU_VALIDATE_CALL = 0x00099867u,
    RVA_USE_INTERNAL_VALIDATE_CALL = 0x000b4828u,
    RVA_QUICK_MENU_SPIRIT_VALIDATE_CALL = 0x000998b9u,
    RVA_QUICK_MENU_SPIRIT_ACTIVATE_CALL = 0x000998dcu,
    RVA_SPIRIT_STRIKE_VALIDATE = 0x00010940u,
    RVA_SPIRIT_STRIKE_ACTIVATE = 0x0000fba0u,
    RVA_CHARACTER_INPUT_HANDLER = 0x000277b0u,
    RVA_CHARACTER_INPUT_VTABLE_SLOT = 0x002c9f84u,
    RVA_APPLY_DAMAGE = 0x000d21d0u,
    RVA_COLLISION_DAMAGE = 0x00138870u,
    RVA_CONTROLLER_UPDATE = 0x00027cf0u,
    RVA_CONTROLLER_UPDATE_VTABLE_SLOT = 0x002c9f60u,
    RVA_GROUP_PLAYERS_PREVIOUS_CHARACTER = 0x00023f60u,
    RVA_GROUP_PLAYERS_NEXT_CHARACTER = 0x00024060u,
    RVA_ARBITER_MOVEMENT = 0x000dae80u,
    RVA_PLAYER_MOVE_CALL_ALTERNATE = 0x00028e3fu,
    RVA_PLAYER_MOVE_CALL_NORMAL = 0x00028e5eu,
    RVA_MAIN_FRAME_UPDATE_CALL = 0x0028ddbau,
    RVA_MAIN_FRAME_UPDATE = 0x0028d3f0u,
    RVA_RENDER_FIRST_PHASE = 0x001d48c0u,
    RVA_RENDER_FIRST_PHASE_CALL_MAIN = 0x0028d45bu,
    RVA_RENDER_START = 0x001dce30u,
    RVA_RENDER_START_CALL_MAIN = 0x0028d443u,
    RVA_RENDER_PHASE = 0x001d4750u,
    RVA_RENDER_PHASE_CALL_MAIN = 0x0028d473u,
    RVA_FRAME_END = 0x001dd540u,
    RVA_FRAME_END_CALL_MAIN = 0x0028d58cu,
    RVA_PC_QUIT_SCREEN_GLOBAL = 0x00408d68u,
    RVA_PC_QUIT_SCREEN_SHOW = 0x0001dbe0u,
    RVA_PC_QUIT_SCREEN_RENDER = 0x0001d690u,
    RVA_PC_QUIT_SCREEN_RENDER_CALL = 0x0028d572u,
    RVA_QUICK_MENU_IS_ACTIVE = 0x0009c330u,
    RVA_QUICK_MENU_GLOBAL = 0x003c2f84u,
    RVA_QUICK_MENU_RENDER_SUBMIT = 0x0009bba0u,
    RVA_QUICK_MENU_RENDER_SUBMIT_VTABLE_SLOT = 0x002caf28u,
    RVA_INGAME_UI_CONTROLLER_GLOBAL = 0x003c2f88u,
    RVA_SHOP_LAYER_GLOBAL = 0x003c2f70u,
    RVA_BLACKSMITH_LAYER_GLOBAL = 0x003c2f74u,
    RVA_WORLD_SCENE_GLOBAL = 0x00408d1cu,
    RVA_SHOP_IS_ACTIVE = 0x0008d1c0u,
    RVA_BLACKSMITH_START = 0x00092c40u,
    RVA_BLACKSMITH_IS_ACTIVE = 0x00092c60u,
    RVA_BLACKSMITH_ACTIVE_EXPORT_ENTRY = 0x0030d414u,
    RVA_BLACKSMITH_START_EXPORT_ENTRY = 0x0030d418u,
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
    RVA_UI_SCENE_RENDER = 0x0000a820u,
    RVA_UI_SCENE_RENDER_CALL = 0x0000a760u,
    RVA_HUD_PARTY_POINTER_COPY = 0x000015b0u,
    RVA_HUD_GROUP_VALUES_POINTER_CALL = 0x00181517u,
    RVA_HUD_GIZMO_PORTRAIT_POINTER_CALL = 0x000aab3au,
    RVA_HUD_PORTRAIT_RESOURCE_ASSIGNMENT = 0x0015c0e0u,
    RVA_HUD_PORTRAIT_RESOURCE_ASSIGNMENT_CALL = 0x000aac08u,
    RVA_CHARACTER_TYPE_TO_PORTRAIT_ENUM = 0x0003f430u,
    RVA_CHARACTER_TYPE_TO_PORTRAIT_LOOKUP = 0x0003f498u,
    RVA_HUD_PORTRAIT_RESOURCE_SELECT = 0x0015c070u,
    RVA_UI_RESOURCE_TABLE_INITIALIZED = 0x003c2fefu,
    RVA_HUD_GIZMO_VALUES_POINTER_CALL = 0x000a9d5bu,
    RVA_HUD_GIZMO_NAME_POINTER_CALL = 0x000a9e15u,
    RVA_HUD_GIZMO_STATUS_POINTER_CALL = 0x000aacabu,
    RVA_MINIMAP_UPDATE_POINTER_CALL = 0x00087760u,
    RVA_MINIMAP_SNAPSHOT_POINTER_CALL = 0x00087a27u,
    RVA_MINIMAP_RENDER_POINTER_CALL = 0x00087af7u,
    RVA_RENDER_PHASE_CALL_WORLD_PREPASS = 0x0000a62du,
    RVA_RENDER_PHASE_CALL_WORLD = 0x0000a689u,
    RVA_RENDER_PHASE_CALL_WORLD_OFFSET = 0x0000a738u,
    RVA_STOP_RUMBLE = 0x000b50d0u,
    RVA_STOP_RUMBLE_CALL = 0x000b4f23u,
    RVA_SCRIPT_CALL_OPCODE = 0x001c4970u,
    RVA_SCRIPT_CALL_OPCODE_SLOT = 0x00323fa0u,
    RVA_SCRIPT_METHOD_OPCODE = 0x001c4b10u,
    RVA_SCRIPT_METHOD_OPCODE_SLOT = 0x00323fa4u,
    RVA_SCRIPT_METHOD_BINDING_CALL = 0x001c4c2fu,
    RVA_SCRIPT_BINDING_INVOKE = 0x002351c0u,
    RVA_CAMERA_MANAGER_SET_RENDER_CAMERA = 0x00036fb0u,
    RVA_CAMERA_INPUT_EVENT = 0x000e85f0u,
    RVA_CAMERA_INPUT_EVENT_VTABLE_SLOT = 0x002cce5cu,
    RVA_MOTION_BLUR_POST_RENDER = 0x001de0b0u,
    RVA_SCREENSHOT_POST_RENDER = 0x001de7b0u,
    RVA_MOTION_BLUR_POST_RENDER_VTABLE_SLOT = 0x002dd930u,
    RVA_SCREENSHOT_POST_RENDER_VTABLE_SLOT = 0x002dd910u,
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
    RVA_SKILL_DATA_AVAILABLE = 0x000da2a0u,
    RVA_SKILL_AVAILABILITY_FLAG = 0x003c2fd9u,
    RVA_SKILL_VALIDATE_FLAG = 0x0034a8b0u,
    RVA_ZONE_SET_NOW = 0x00007910u,
    RVA_ZONE_ENTER = 0x00007970u,
    RVA_ZONE_SWITCH_NOW = 0x00007990u,
    RVA_ZONE_LOAD = 0x00007b80u,
    RVA_ZONE_SWITCH_MAIN = 0x00006380u,
    RVA_ZONE_DOOR_ACTIVATE = 0x000ce3a0u,
    RVA_ZONE_ENTER_TEMPORARY = 0x000064b0u,
    RVA_ZONE_EXIT_TEMPORARY = 0x00006710u,
    RVA_ZONE_SET_PLAYER_POSITION = 0x00104ed0u,
    RVA_ZONE_INTERNAL_POSITION_SETTER = 0x00003050u,
    RVA_ZONE_ENTER_LEAD_POP_CALL = 0x00005c59u,
    RVA_ZONE_EXIT_LEAD_MOVE_CALL = 0x000068d3u,
    RVA_ZONE_POP_TO_NAMED_LOCATION = 0x000f63d0u,
    RVA_ZONE_EXIT_LEAD_MOVE = 0x000f30a0u,
    RVA_ZONE_FORMATION_POP_MEMBERS = 0x000f6260u,
    RVA_ZONE_SET_MODE_LEAD_ONLY = 0x00024720u,
    RVA_ZONE_SET_MODE_FULL_PARTY = 0x00024850u,
    RVA_ZONE_SHOW_PARTY_MEMBERS = 0x00024950u,
    RVA_ZONE_HIDE_PARTY_MEMBERS = 0x00024a70u,
    RVA_ZONE_AI_MANAGER_GLOBAL = 0x00409de4u
};

typedef struct ExpectedExport {
    uint32_t slot_rva;
    uint32_t function_rva;
} ExpectedExport;

typedef struct ExpectedEntry {
    uint32_t function_rva;
    uint8_t bytes[16];
    size_t byte_count;
    const char *name;
} ExpectedEntry;

static const ExpectedExport expected_exports[] = {
    {0x0030c570u, 0x000d3ae0u},
    {0x0030c698u, 0x000c89c0u},
    {0x0030c69cu, 0x000c8a00u},
    {0x0030ceb4u, 0x000e0460u},
    {0x0030d3d0u, 0x0000f380u},
    {0x0030d3d4u, 0x0000f310u},
    {0x0030d3d8u, 0x0003af80u},
    {0x0030d3dcu, 0x0000f2e0u},
    {0x0030d3e0u, 0x0000f2e0u},
    {0x0030d3e4u, 0x0000f480u},
    {0x0030d3e8u, 0x0000f420u},
    {0x0030d3ecu, 0x0003aff0u},
    {0x0030d3f0u, 0x0000f3f0u},
    {0x0030d3f4u, 0x0000f3f0u}
};

static const ExpectedEntry expected_cleanroom_entries[] = {
    {0x000b1b00u, {0x83,0xec,0x3c,0x56,0x57}, 5u,
        "InternalSpawnPC(ResourceName, xyz)"},
    {0x000b23a0u, {0x81,0xec,0x14,0x01,0x00,0x00,0x56,0x57}, 8u,
        "RemovePC(ResourceName)"},
    {0x000b20d0u, {0x55,0x8b,0xec,0x83,0xe4,0xf8}, 6u,
        "SpawnEntity(name, xyz)"},
    {0x000b2300u, {0x55,0x8b,0xec,0x83,0xe4,0xf8}, 6u,
        "DespawnEntity"},
    {0x00104480u, {0x51,0x8b,0x44,0x24,0x08,0x50}, 6u,
        "GetPC(text)"},
    {0x00104400u, {0x83,0xec,0x0c,0xa1,0x8c,0x9d,0x80,0x00}, 8u,
        "GetGenericEntity(text)"},
    {0x001b9440u, {0x57,0x8b,0xf8,0x8b,0x06,0x25,0x80,0xef}, 8u,
        "ResourceName from text"},
    {0x001b9760u, {0x85,0xc0,0x74,0x2f,0x53,0x8d,0x58,0xfc}, 8u,
        "ResourceName reference release"},
    {0x00025100u, {0xa1,0x94,0x8d,0x80,0x00,0xc3}, 6u,
        "GetGroupPlayers"},
    {RVA_GROUP_PLAYERS_PREVIOUS_CHARACTER,
        {0x55,0x8b,0xec,0x83,0xe4,0xf8,0x83,0xec,
         0x18,0x83,0xbe,0xcc,0x00,0x00,0x00,0x01}, 16u,
        "CGroupPlayers::PreviousCharacter consumer"},
    {RVA_GROUP_PLAYERS_NEXT_CHARACTER,
        {0x55,0x8b,0xec,0x83,0xe4,0xf8,0x83,0xec,
         0x18,0x83,0xbe,0xcc,0x00,0x00,0x00,0x01}, 16u,
        "CGroupPlayers::NextCharacter consumer"},
    {0x00004fa0u, {0x8a,0x81,0xd4,0x00,0x00,0x00,0xc3}, 7u,
        "CGroupPlayers::InCombat"},
    {0x00024480u, {0x53,0x8b,0x5c,0x24,0x10,0x55,0x8b,0xe9}, 8u,
        "CGroupPlayers combat event transition"},
    {0x0002a880u, {0xa1,0xa8,0x8d,0x80,0x00,0x85,0xc0,0x74}, 8u,
        "SetFirstPersonCameraMode"},
    {0x000b5320u, {0xc6,0x05,0xcc,0x2f,0x7c,0x00,0x01,0xc3}, 8u,
        "NoSpNeeded"},
    {0x0000f5b0u, {0xc6,0x05,0x23,0x2f,0x7c,0x00,0x01,0xc3}, 8u,
        "NoSspNeeded"},
    {0x0000f5e0u, {0x51,0xa1,0x30,0x8d,0x80,0x00,0x85,0xc0}, 8u,
        "GetSsp"},
    {0x0000f5c0u, {0xd9,0x44,0x24,0x04,0x51,0x8b,0x0d,0x30}, 8u,
        "SetSsp"},
    {0x000204d0u, {0x55,0x8b,0xec,0x83,0xe4,0xf8,0x83,0xec}, 8u,
        "FillInventory"},
    {0x000113a0u, {0xa1,0x30,0x8d,0x80,0x00,0x56,0x8b,0x74}, 8u,
        "SpiritStrikeEnable"},
    {0x000c1270u, {0x83,0xec,0x0c,0x56,0x8b,0x74,0x24,0x14}, 8u,
        "GetCharacterNumberStat"},
    {0x000c1350u, {0x83,0xec,0x0c,0x53,0x56,0x8b,0x74,0x24}, 8u,
        "SetCharacterNumberStat"},
    {0x000d8790u, {0x8b,0x44,0x24,0x04,0x56,0x8b,0xf1,0x83}, 8u,
        "CCharacterWeapon::SetWeapon"},
    {0x000dca10u, {0x80,0x7c,0x24,0x04,0x00,0x74,0x05,0xfe}, 8u,
        "CCharacterArbiter::GELSetInvulnerable"},
    {0x0028be90u, {0xd9,0x44,0x24,0x04,0xd9,0x1d,0x10,0x58}, 8u,
        "SetMasterGameSpeed"},
    {0x000d8280u, {0x53,0x55,0x8b,0x6c,0x24,0x0c,0x56,0x57}, 8u,
        "ranged weapon reattach after model switch"}
};

static uint8_t *read_file(const wchar_t *path, DWORD *file_size) {
    HANDLE file;
    uint8_t *data;
    DWORD bytes_read;

    file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        return NULL;
    }
    *file_size = GetFileSize(file, NULL);
    if (*file_size == INVALID_FILE_SIZE) {
        CloseHandle(file);
        return NULL;
    }
    data = (uint8_t *)HeapAlloc(GetProcessHeap(), 0, *file_size);
    if (data == NULL || !ReadFile(file, data, *file_size, &bytes_read, NULL) ||
        bytes_read != *file_size) {
        HeapFree(GetProcessHeap(), 0, data);
        CloseHandle(file);
        return NULL;
    }
    CloseHandle(file);
    return data;
}

static uint8_t *map_pe_image(const uint8_t *file, DWORD file_size) {
    const IMAGE_DOS_HEADER *dos;
    const IMAGE_NT_HEADERS32 *nt;
    const IMAGE_SECTION_HEADER *sections;
    uint8_t *image;
    WORD index;

    if (file == NULL || file_size < sizeof(IMAGE_DOS_HEADER)) {
        return NULL;
    }
    dos = (const IMAGE_DOS_HEADER *)file;
    if (dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew <= 0 ||
        (DWORD)dos->e_lfanew > file_size - sizeof(IMAGE_NT_HEADERS32)) {
        return NULL;
    }
    nt = (const IMAGE_NT_HEADERS32 *)(file + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE ||
        nt->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR32_MAGIC ||
        nt->OptionalHeader.SizeOfHeaders > file_size) {
        return NULL;
    }

    image = (uint8_t *)VirtualAlloc(NULL, nt->OptionalHeader.SizeOfImage,
        MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (image == NULL) {
        return NULL;
    }
    memcpy(image, file, nt->OptionalHeader.SizeOfHeaders);
    sections = IMAGE_FIRST_SECTION(nt);
    for (index = 0; index < nt->FileHeader.NumberOfSections; ++index) {
        DWORD source = sections[index].PointerToRawData;
        DWORD size = sections[index].SizeOfRawData;
        DWORD destination = sections[index].VirtualAddress;
        if (source > file_size || size > file_size - source ||
            destination > nt->OptionalHeader.SizeOfImage ||
            size > nt->OptionalHeader.SizeOfImage - destination) {
            VirtualFree(image, 0, MEM_RELEASE);
            return NULL;
        }
        if (size != 0) {
            memcpy(image + destination, file + source, size);
        }
    }
    return image;
}

static const uint8_t *relative_call_target(const uint8_t *instruction) {
    int32_t displacement;
    if (instruction[0] != 0xe8) {
        return NULL;
    }
    memcpy(&displacement, instruction + 1, sizeof(displacement));
    return instruction + 5 + displacement;
}

typedef struct ZonePatchProbe {
    uint32_t rva;
    size_t size;
    uint8_t original[16];
    const char *name;
} ZonePatchProbe;

static void point_relative_call(uint8_t *instruction, const uint8_t *target) {
    int32_t displacement = (int32_t)(target - (instruction + 5u));

    instruction[0] = 0xe8u;
    memcpy(instruction + 1u, &displacement, sizeof(displacement));
}

static void check_blacksmith_ui_adapter_exact_image(
    uint8_t *image,
    int *failures
) {
    uint8_t start_original[7];
    uint8_t active_original[7];
    uint8_t mismatched_byte;

    memcpy(start_original, image + RVA_BLACKSMITH_START,
        sizeof(start_original));
    memcpy(active_original, image + RVA_BLACKSMITH_IS_ACTIVE,
        sizeof(active_original));

    if (*(const uint32_t *)(
            image + RVA_BLACKSMITH_START_EXPORT_ENTRY) !=
            RVA_BLACKSMITH_START ||
        *(const uint32_t *)(
            image + RVA_BLACKSMITH_ACTIVE_EXPORT_ENTRY) !=
            RVA_BLACKSMITH_IS_ACTIVE ||
        !SudekiMpBlacksmithUiLoadedStartSignaturesMatch(
            image,
            RVA_BLACKSMITH_START_EXPORT_ENTRY + sizeof(uint32_t),
            (uintptr_t)image)) {
        fputs("FAIL: relocated Blacksmith Start/Active exact gate mismatch\n",
            stderr);
        ++*failures;
        return;
    }

    if (!SudekiMpInstallBlacksmithUiAdapter((HMODULE)image, FALSE) ||
        memcmp(image + RVA_BLACKSMITH_START, start_original,
            sizeof(start_original)) != 0 ||
        memcmp(image + RVA_BLACKSMITH_IS_ACTIVE, active_original,
            sizeof(active_original)) != 0) {
        fputs("FAIL: disabled Blacksmith adapter changed the exact image\n",
            stderr);
        ++*failures;
        SudekiMpUninstallBlacksmithUiAdapter();
    }

    SudekiMpBlacksmithUiAdapterInjectSecondHookFailureForTest(TRUE);
    SetLastError(ERROR_SUCCESS);
    if (SudekiMpInstallBlacksmithUiAdapter((HMODULE)image, TRUE)) {
        fputs("FAIL: injected second Blacksmith hook failure was ignored\n",
            stderr);
        ++*failures;
        SudekiMpUninstallBlacksmithUiAdapter();
    } else if (GetLastError() != ERROR_GEN_FAILURE ||
        memcmp(image + RVA_BLACKSMITH_START, start_original,
            sizeof(start_original)) != 0 ||
        memcmp(image + RVA_BLACKSMITH_IS_ACTIVE, active_original,
            sizeof(active_original)) != 0 ||
        SudekiMpBlacksmithUiAdapterActive()) {
        fputs("FAIL: partial paired-hook failure did not roll back Start\n",
            stderr);
        ++*failures;
        SudekiMpUninstallBlacksmithUiAdapter();
    }

    mismatched_byte = image[RVA_BLACKSMITH_IS_ACTIVE + 5u];
    image[RVA_BLACKSMITH_IS_ACTIVE + 5u] ^= 0xffu;
    SetLastError(ERROR_SUCCESS);
    if (SudekiMpInstallBlacksmithUiAdapter((HMODULE)image, TRUE) ||
        GetLastError() != ERROR_BAD_EXE_FORMAT ||
        memcmp(image + RVA_BLACKSMITH_START, start_original,
            sizeof(start_original)) != 0) {
        fputs("FAIL: Blacksmith Active mismatch did not reject atomically\n",
            stderr);
        ++*failures;
        SudekiMpUninstallBlacksmithUiAdapter();
    }
    image[RVA_BLACKSMITH_IS_ACTIVE + 5u] = mismatched_byte;

    mismatched_byte = image[RVA_BLACKSMITH_START + 5u];
    image[RVA_BLACKSMITH_START + 5u] ^= 0xffu;
    SetLastError(ERROR_SUCCESS);
    if (SudekiMpInstallBlacksmithUiAdapter((HMODULE)image, TRUE) ||
        GetLastError() != ERROR_BAD_EXE_FORMAT ||
        memcmp(image + RVA_BLACKSMITH_IS_ACTIVE, active_original,
            sizeof(active_original)) != 0) {
        fputs("FAIL: Blacksmith Start mismatch did not reject atomically\n",
            stderr);
        ++*failures;
        SudekiMpUninstallBlacksmithUiAdapter();
    }
    image[RVA_BLACKSMITH_START + 5u] = mismatched_byte;

    SetLastError(ERROR_SUCCESS);
    if (!SudekiMpInstallBlacksmithUiAdapter((HMODULE)image, TRUE)) {
        fprintf(stderr,
            "FAIL: paired Blacksmith adapter rejected exact image (error=%lu)\n",
            (unsigned long)GetLastError());
        ++*failures;
    } else {
        if (image[RVA_BLACKSMITH_START] != 0xe9u ||
            image[RVA_BLACKSMITH_IS_ACTIVE] != 0xe9u ||
            memcmp(image + RVA_BLACKSMITH_START + 5u,
                start_original + 5u, 2u) != 0 ||
            memcmp(image + RVA_BLACKSMITH_IS_ACTIVE + 5u,
                active_original + 5u, 2u) != 0) {
            fputs("FAIL: paired Blacksmith entry detours were not exact\n",
                stderr);
            ++*failures;
        }
        SudekiMpUninstallBlacksmithUiAdapter();
    }
    if (memcmp(image + RVA_BLACKSMITH_START, start_original,
            sizeof(start_original)) != 0 ||
        memcmp(image + RVA_BLACKSMITH_IS_ACTIVE, active_original,
            sizeof(active_original)) != 0 ||
        SudekiMpBlacksmithUiAdapterActive() ||
        !SudekiMpSplitScreenNativeBlacksmithReportedActive(
            TRUE, FALSE, FALSE, 0u, 0u)) {
        fputs("FAIL: Blacksmith adapter uninstall did not restore native policy\n",
            stderr);
        ++*failures;
    }
}

static void check_blacksmith_roster_actor_identity_policy(int *failures) {
    int actor_marker;
    int other_marker;
    const void *actor = &actor_marker;
    const void *other = &other_marker;

#define CHECK_ROSTER_IDENTITY(expected, expression, label) do { \
    BOOL actual = (expression); \
    if ((actual != FALSE) != (expected)) { \
        fprintf(stderr, "FAIL: Blacksmith roster identity policy %s\n", \
            label); \
        ++*failures; \
    } \
} while (0)
    CHECK_ROSTER_IDENTITY(1,
        SudekiMpSplitScreenRosterActorIdentityPolicy(
            0u, actor, 0x23u, 0x23u, actor, 1u, actor,
            TRUE, TRUE, TRUE, FALSE),
        "rejected exact unique active lease");
    CHECK_ROSTER_IDENTITY(0,
        SudekiMpSplitScreenRosterActorIdentityPolicy(
            2u, actor, 0x23u, 0x23u, actor, 1u, actor,
            TRUE, TRUE, TRUE, FALSE),
        "accepted invalid seat");
    CHECK_ROSTER_IDENTITY(0,
        SudekiMpSplitScreenRosterActorIdentityPolicy(
            0u, actor, 0x01u, 0x23u, actor, 1u, actor,
            TRUE, TRUE, TRUE, FALSE),
        "accepted wrong stable type");
    CHECK_ROSTER_IDENTITY(0,
        SudekiMpSplitScreenRosterActorIdentityPolicy(
            0u, actor, 0x23u, 0x23u, actor, 2u, actor,
            TRUE, TRUE, TRUE, FALSE),
        "accepted duplicate active-party type");
    CHECK_ROSTER_IDENTITY(0,
        SudekiMpSplitScreenRosterActorIdentityPolicy(
            0u, actor, 0x23u, 0x23u, other, 1u, actor,
            TRUE, TRUE, TRUE, FALSE),
        "accepted different resolved actor");
    CHECK_ROSTER_IDENTITY(0,
        SudekiMpSplitScreenRosterActorIdentityPolicy(
            0u, actor, 0x23u, 0x23u, actor, 1u, other,
            TRUE, TRUE, TRUE, FALSE),
        "accepted stale locked actor");
    CHECK_ROSTER_IDENTITY(0,
        SudekiMpSplitScreenRosterActorIdentityPolicy(
            0u, actor, 0x23u, 0x23u, actor, 1u, actor,
            FALSE, TRUE, TRUE, FALSE),
        "accepted released runtime");
    CHECK_ROSTER_IDENTITY(0,
        SudekiMpSplitScreenRosterActorIdentityPolicy(
            0u, actor, 0x23u, 0x23u, actor, 1u, actor,
            TRUE, FALSE, TRUE, FALSE),
        "accepted released roles");
    CHECK_ROSTER_IDENTITY(0,
        SudekiMpSplitScreenRosterActorIdentityPolicy(
            0u, actor, 0x23u, 0x23u, actor, 1u, actor,
            TRUE, TRUE, FALSE, FALSE),
        "accepted dropped participation");
    CHECK_ROSTER_IDENTITY(0,
        SudekiMpSplitScreenRosterActorIdentityPolicy(
            0u, actor, 0x23u, 0x23u, actor, 1u, actor,
            TRUE, TRUE, TRUE, TRUE),
        "accepted transition quarantine");
#undef CHECK_ROSTER_IDENTITY
}

static void check_shared_interaction_modal_runtime(
    uint8_t *image,
    int *failures
) {
    union {
        void *alignment;
        uint8_t bytes[0xc0u];
    } controller_storage;
    union {
        void *alignment;
        uint8_t bytes[0x8cu];
    } shop_storage;
    union {
        void *alignment;
        uint8_t bytes[0x318u];
    } blacksmith_storage;
    uint8_t *controller = controller_storage.bytes;
    uint8_t *shop = shop_storage.bytes;
    uint8_t *blacksmith = blacksmith_storage.bytes;
    SudekiMpPlayerStatehoodSnapshot snapshot;

    ZeroMemory(&controller_storage, sizeof(controller_storage));
    ZeroMemory(&shop_storage, sizeof(shop_storage));
    ZeroMemory(&blacksmith_storage, sizeof(blacksmith_storage));
    *(void **)controller = image + RVA_INGAME_UI_CONTROLLER_VTABLE;
    *(void **)shop = image + RVA_SHOP_LAYER_VTABLE;
    *(void **)blacksmith = image + RVA_BLACKSMITH_LAYER_VTABLE;
    *(void **)(controller + 0x74u) = shop;
    *(void **)(controller + 0x78u) = blacksmith;
    *(void **)(image + RVA_INGAME_UI_CONTROLLER_GLOBAL) = controller;
    *(void **)(image + RVA_SHOP_LAYER_GLOBAL) = shop;
    *(void **)(image + RVA_BLACKSMITH_LAYER_GLOBAL) = blacksmith;
    SudekiMpPlayerStatehoodInitialize(SudekiMpPlayerStatehoodRuntime());

    if (!SudekiMpInstallSplitScreenRender(
            (HMODULE)image,
            FALSE,
            FALSE,
            0u,
            FALSE,
            FALSE,
            FALSE,
            FALSE,
            FALSE,
            0.20f,
            2.25f,
            1.50f,
            0.65f)) {
        fprintf(stderr,
            "FAIL: shared modal detector install rejected exact image (error=%lu)\n",
            (unsigned long)GetLastError());
        ++*failures;
        goto restore_globals;
    }
    if (SudekiMpSplitScreenSharedInteractionModalActive()) {
        fputs("FAIL: inactive native Shop/Blacksmith reported active\n",
            stderr);
        ++*failures;
    }
    *(unsigned int *)(controller + 0xb8u) = 9u;
    if (!SudekiMpSplitScreenSharedInteractionModalActive() ||
        !SudekiMpPlayerStatehoodGetSnapshot(
            SudekiMpPlayerStatehoodRuntime(),
            GetTickCount(),
            &snapshot) ||
        snapshot.state != SUDEKIMP_INTERACTION_SESSION_ACTIVE ||
        snapshot.provenance.kind != SUDEKIMP_INTERACTION_SHOP ||
        snapshot.provenance.player_index != 0u) {
        fputs("FAIL: Shop closing-mode full-width/statehood observation mismatch\n",
            stderr);
        ++*failures;
    }
    *(unsigned int *)(controller + 0xb8u) = 0u;
    if (SudekiMpSplitScreenSharedInteractionModalActive()) {
        fputs("FAIL: no-cache Shop close retained a recovery barrier\n",
            stderr);
        ++*failures;
    }
    blacksmith[0x29u] = 0u;
    if (SudekiMpSplitScreenSharedInteractionModalActive()) {
        fputs("FAIL: zero AL Blacksmith getter was widened as active\n",
            stderr);
        ++*failures;
    }
    blacksmith[0x29u] = 1u;
    if (!SudekiMpSplitScreenSharedInteractionModalActive() ||
        !SudekiMpPlayerStatehoodGetSnapshot(
            SudekiMpPlayerStatehoodRuntime(),
            GetTickCount(),
            &snapshot) ||
        snapshot.provenance.kind != SUDEKIMP_INTERACTION_BLACKSMITH ||
        snapshot.provenance.player_index != 0u) {
        fputs("FAIL: Blacksmith full-width/statehood observation mismatch\n",
            stderr);
        ++*failures;
    }
    blacksmith[0x29u] = 0u;
    if (SudekiMpSplitScreenSharedInteractionModalActive()) {
        fputs("FAIL: no-cache Blacksmith close retained a recovery barrier\n",
            stderr);
        ++*failures;
    }
    SudekiMpUninstallSplitScreenRender();
    if (SudekiMpSplitScreenSharedInteractionModalActive()) {
        fputs("FAIL: uninstalled modal inspector retained quiescence\n",
            stderr);
        ++*failures;
    }

restore_globals:
    *(void **)(image + RVA_INGAME_UI_CONTROLLER_GLOBAL) = NULL;
    *(void **)(image + RVA_SHOP_LAYER_GLOBAL) = NULL;
    *(void **)(image + RVA_BLACKSMITH_LAYER_GLOBAL) = NULL;
}

static void capture_zone_patch_probes(
    ZonePatchProbe *probes,
    size_t count,
    const uint8_t *image
) {
    size_t index;

    for (index = 0u; index < count; ++index) {
        memcpy(probes[index].original, image + probes[index].rva,
            probes[index].size);
    }
}

static void check_zone_patch_probes_restored(
    const ZonePatchProbe *probes,
    size_t count,
    const uint8_t *image,
    int *failures
) {
    size_t index;

    for (index = 0u; index < count; ++index) {
        if (memcmp(image + probes[index].rva, probes[index].original,
                probes[index].size) != 0) {
            fprintf(stderr,
                "FAIL: zone transition hook did not restore %s\n",
                probes[index].name);
            ++*failures;
        }
    }
}

static void test_zone_transition_exact_image(
    uint8_t *image,
    int *failures
) {
    static const uint8_t formation_tail[] = {
        0x85u,0xc0u,0x74u,0x10u,0x05u,0xf4u,0x00u,0x00u,
        0x00u,0x74u,0x09u,0x6au,0x00u,0x6au,0x00u
    };
    static const uint8_t set_mode_lead_only_entry[] = {
        0x83u,0xecu,0x14u,0x83u,0xb9u,0xd0u,0x00u,
        0x00u,0x00u,0x00u,0x89u,0x4cu,0x24u,0x04u
    };
    static const uint8_t set_mode_full_party_entry[] = {
        0x83u,0xecu,0x10u,0x53u,0x8bu,0xd9u,0x83u,
        0xbbu,0xd0u,0x00u,0x00u,0x00u,0x01u
    };
    static const uint8_t party_visibility_entry[] = {
        0x83u,0xecu,0x10u,0x53u,0x55u,0x56u,0x57u,
        0x8du,0xa9u,0x9cu,0x00u,0x00u,0x00u
    };
    ZonePatchProbe probes[] = {
        {RVA_ZONE_SET_NOW, 7u, {0}, "SetZoneNow"},
        {RVA_ZONE_ENTER, 5u, {0}, "EnterZone"},
        {RVA_ZONE_SWITCH_NOW, 5u, {0}, "SwitchZoneNow"},
        {RVA_ZONE_LOAD, 10u, {0}, "LoadZone"},
        {RVA_ZONE_SWITCH_MAIN, 7u, {0}, "SwitchMainZone"},
        {RVA_ZONE_DOOR_ACTIVATE, 8u, {0}, "DoorActivateFromScript"},
        {RVA_ZONE_ENTER_TEMPORARY, 9u, {0}, "EnterTemporaryZone"},
        {RVA_ZONE_EXIT_TEMPORARY, 12u, {0}, "ExitTemporaryZone"},
        {RVA_ZONE_SET_PLAYER_POSITION, 5u, {0}, "SetPlayerPosition"},
        {RVA_ZONE_INTERNAL_POSITION_SETTER, 5u, {0},
            "InternalPositionSetter"},
        {RVA_ZONE_HIDE_PARTY_MEMBERS, sizeof(party_visibility_entry), {0},
            "HidePartyMembers"},
        {RVA_ZONE_ENTER_LEAD_POP_CALL, 5u, {0},
            "enter lead placement call"},
        {RVA_ZONE_EXIT_LEAD_MOVE_CALL, 5u, {0},
            "exit lead placement call"}
    };
    const size_t probe_count = sizeof(probes) / sizeof(probes[0]);
    uint8_t saved_call[5];
    uint8_t saved_set_mode_lead_only_byte;
    uint8_t saved_set_mode_full_party_byte;
    uint8_t saved_show_party_members_byte;
    uint8_t saved_hide_party_members_byte;
    uint32_t relocated_ai_manager;
    size_t index;

    if (!SudekiMpZoneTransitionShouldArmTemporaryExit(TRUE, 0u, 4u) ||
        SudekiMpZoneTransitionShouldArmTemporaryExit(FALSE, 0u, 4u) ||
        SudekiMpZoneTransitionShouldArmTemporaryExit(TRUE, 1u, 4u) ||
        SudekiMpZoneTransitionShouldArmTemporaryExit(TRUE, 0u, 3u) ||
        SudekiMpZoneTransitionShouldArmTemporaryExit(TRUE, 0u, 0u)) {
        fputs("FAIL: temporary-exit policy confused save-load cleanup with a real co-op exit\n",
            stderr);
        ++*failures;
    }
    if (!SudekiMpZoneTransitionShouldDeferExitLeadPlacement(TRUE, TRUE) ||
        SudekiMpZoneTransitionShouldDeferExitLeadPlacement(FALSE, TRUE) ||
        SudekiMpZoneTransitionShouldDeferExitLeadPlacement(TRUE, FALSE) ||
        SudekiMpZoneTransitionShouldDeferExitLeadPlacement(FALSE, FALSE)) {
        fputs("FAIL: temporary-exit lead placement was not always deferred from the inline callsite\n",
            stderr);
        ++*failures;
    }
    if (SudekiMpZoneTransitionExitPresentationAction(
            FALSE, FALSE, FALSE, FALSE, FALSE) !=
                SUDEKIMP_EXIT_PRESENTATION_READY ||
        SudekiMpZoneTransitionExitPresentationAction(
            TRUE, TRUE, TRUE, FALSE, FALSE) !=
                SUDEKIMP_EXIT_PRESENTATION_READY ||
        SudekiMpZoneTransitionExitPresentationAction(
            TRUE, TRUE, FALSE, TRUE, FALSE) !=
                SUDEKIMP_EXIT_PRESENTATION_SHOW_OWNED ||
        SudekiMpZoneTransitionExitPresentationAction(
            TRUE, TRUE, FALSE, TRUE, TRUE) !=
                SUDEKIMP_EXIT_PRESENTATION_WAIT ||
        SudekiMpZoneTransitionExitPresentationAction(
            TRUE, FALSE, FALSE, TRUE, FALSE) !=
                SUDEKIMP_EXIT_PRESENTATION_WAIT ||
        SudekiMpZoneTransitionExitPresentationAction(
            TRUE, TRUE, FALSE, FALSE, FALSE) !=
                SUDEKIMP_EXIT_PRESENTATION_WAIT) {
        fputs("FAIL: temporary-exit owned presentation lease policy mismatch\n",
            stderr);
        ++*failures;
    }
    if (SudekiMpZoneTransitionDoorwayStagingAction(
            TRUE, TRUE, TRUE, TRUE, TRUE, FALSE, 1.0f, 2.0f) !=
                SUDEKIMP_DOORWAY_STAGING_WAIT ||
        SudekiMpZoneTransitionDoorwayStagingAction(
            TRUE, TRUE, TRUE, TRUE, TRUE, FALSE, 2.0f, 2.0f) !=
                SUDEKIMP_DOORWAY_STAGING_REPOP_NOW ||
        SudekiMpZoneTransitionDoorwayStagingAction(
            FALSE, TRUE, TRUE, TRUE, TRUE, FALSE, 2.0f, 2.0f) !=
                SUDEKIMP_DOORWAY_STAGING_ACCEPT_FIRST ||
        SudekiMpZoneTransitionDoorwayStagingAction(
            TRUE, FALSE, TRUE, TRUE, TRUE, FALSE, 2.0f, 2.0f) !=
                SUDEKIMP_DOORWAY_STAGING_ACCEPT_FIRST ||
        SudekiMpZoneTransitionDoorwayStagingAction(
            TRUE, TRUE, FALSE, TRUE, TRUE, FALSE, 2.0f, 2.0f) !=
                SUDEKIMP_DOORWAY_STAGING_ACCEPT_FIRST ||
        SudekiMpZoneTransitionDoorwayStagingAction(
            TRUE, TRUE, TRUE, FALSE, TRUE, FALSE, 2.0f, 2.0f) !=
                SUDEKIMP_DOORWAY_STAGING_ACCEPT_FIRST ||
        SudekiMpZoneTransitionDoorwayStagingAction(
            TRUE, TRUE, TRUE, TRUE, FALSE, FALSE, 2.0f, 2.0f) !=
                SUDEKIMP_DOORWAY_STAGING_ACCEPT_FIRST ||
        SudekiMpZoneTransitionDoorwayStagingAction(
            TRUE, TRUE, TRUE, TRUE, TRUE, TRUE, 2.0f, 2.0f) !=
                SUDEKIMP_DOORWAY_STAGING_ACCEPT_FIRST ||
        SudekiMpZoneTransitionDoorwayStagingAction(
            TRUE, TRUE, TRUE, TRUE, TRUE, FALSE, 2.0f, 0.0f) !=
                SUDEKIMP_DOORWAY_STAGING_ACCEPT_FIRST) {
        fputs("FAIL: doorway-staging delayed native-pop policy mismatch\n",
            stderr);
        ++*failures;
    }

    if (image[RVA_ZONE_FORMATION_POP_MEMBERS] != 0xa1u ||
        memcmp(image + RVA_ZONE_FORMATION_POP_MEMBERS + 5u,
            formation_tail, sizeof(formation_tail)) != 0 ||
        memcmp(image + RVA_ZONE_SET_MODE_LEAD_ONLY,
            set_mode_lead_only_entry,
            sizeof(set_mode_lead_only_entry)) != 0 ||
        memcmp(image + RVA_ZONE_SET_MODE_FULL_PARTY,
            set_mode_full_party_entry,
            sizeof(set_mode_full_party_entry)) != 0 ||
        memcmp(image + RVA_ZONE_SHOW_PARTY_MEMBERS,
            party_visibility_entry,
            sizeof(party_visibility_entry)) != 0 ||
        memcmp(image + RVA_ZONE_HIDE_PARTY_MEMBERS,
            party_visibility_entry,
            sizeof(party_visibility_entry)) != 0 ||
        relative_call_target(image + RVA_ZONE_ENTER_LEAD_POP_CALL) !=
            image + RVA_ZONE_POP_TO_NAMED_LOCATION ||
        relative_call_target(image + RVA_ZONE_EXIT_LEAD_MOVE_CALL) !=
            image + RVA_ZONE_EXIT_LEAD_MOVE) {
        fputs("FAIL: exact party-transition formation/callsite seam mismatch\n",
            stderr);
        ++*failures;
        return;
    }
    capture_zone_patch_probes(probes, probe_count, image);

    relocated_ai_manager = (uint32_t)(uintptr_t)(
        image + RVA_ZONE_AI_MANAGER_GLOBAL + 4u);
    memcpy(image + RVA_ZONE_FORMATION_POP_MEMBERS + 1u,
        &relocated_ai_manager, sizeof(relocated_ai_manager));
    SetLastError(ERROR_SUCCESS);
    if (SudekiMpInstallZoneTransitionTrace((HMODULE)image, TRUE, FALSE) ||
        GetLastError() != ERROR_INVALID_DATA) {
        fputs("FAIL: party-transition install accepted a mismatched relocated formation global\n",
            stderr);
        ++*failures;
        SudekiMpUninstallZoneTransitionTrace();
    }
    check_zone_patch_probes_restored(
        probes, probe_count, image, failures);

    relocated_ai_manager = (uint32_t)(uintptr_t)(
        image + RVA_ZONE_AI_MANAGER_GLOBAL);
    memcpy(image + RVA_ZONE_FORMATION_POP_MEMBERS + 1u,
        &relocated_ai_manager, sizeof(relocated_ai_manager));

    memcpy(saved_call, image + RVA_ZONE_ENTER_LEAD_POP_CALL,
        sizeof(saved_call));
    point_relative_call(image + RVA_ZONE_ENTER_LEAD_POP_CALL,
        image + RVA_ZONE_POP_TO_NAMED_LOCATION + 1u);
    SetLastError(ERROR_SUCCESS);
    if (SudekiMpInstallZoneTransitionTrace((HMODULE)image, TRUE, FALSE) ||
        GetLastError() != ERROR_INVALID_DATA) {
        fputs("FAIL: party-transition install accepted a mismatched enter callsite\n",
            stderr);
        ++*failures;
        SudekiMpUninstallZoneTransitionTrace();
    }
    memcpy(image + RVA_ZONE_ENTER_LEAD_POP_CALL, saved_call,
        sizeof(saved_call));
    check_zone_patch_probes_restored(
        probes, probe_count, image, failures);

    saved_set_mode_lead_only_byte =
        image[RVA_ZONE_SET_MODE_LEAD_ONLY +
            sizeof(set_mode_lead_only_entry) - 1u];
    image[RVA_ZONE_SET_MODE_LEAD_ONLY +
        sizeof(set_mode_lead_only_entry) - 1u] ^= 0x01u;
    SetLastError(ERROR_SUCCESS);
    if (SudekiMpInstallZoneTransitionTrace((HMODULE)image, TRUE, FALSE) ||
        GetLastError() != ERROR_INVALID_DATA) {
        fputs("FAIL: party-transition install accepted a mismatched SetModeLeadOnly entry\n",
            stderr);
        ++*failures;
        SudekiMpUninstallZoneTransitionTrace();
    }
    image[RVA_ZONE_SET_MODE_LEAD_ONLY +
        sizeof(set_mode_lead_only_entry) - 1u] =
            saved_set_mode_lead_only_byte;
    check_zone_patch_probes_restored(
        probes, probe_count, image, failures);

    saved_set_mode_full_party_byte =
        image[RVA_ZONE_SET_MODE_FULL_PARTY +
            sizeof(set_mode_full_party_entry) - 1u];
    image[RVA_ZONE_SET_MODE_FULL_PARTY +
        sizeof(set_mode_full_party_entry) - 1u] ^= 0x01u;
    SetLastError(ERROR_SUCCESS);
    if (SudekiMpInstallZoneTransitionTrace((HMODULE)image, TRUE, FALSE) ||
        GetLastError() != ERROR_INVALID_DATA) {
        fputs("FAIL: party-transition install accepted a mismatched SetModeFullParty entry\n",
            stderr);
        ++*failures;
        SudekiMpUninstallZoneTransitionTrace();
    }
    image[RVA_ZONE_SET_MODE_FULL_PARTY +
        sizeof(set_mode_full_party_entry) - 1u] =
            saved_set_mode_full_party_byte;
    check_zone_patch_probes_restored(
        probes, probe_count, image, failures);

    saved_show_party_members_byte =
        image[RVA_ZONE_SHOW_PARTY_MEMBERS +
            sizeof(party_visibility_entry) - 1u];
    image[RVA_ZONE_SHOW_PARTY_MEMBERS +
        sizeof(party_visibility_entry) - 1u] ^= 0x01u;
    SetLastError(ERROR_SUCCESS);
    if (SudekiMpInstallZoneTransitionTrace((HMODULE)image, TRUE, FALSE) ||
        GetLastError() != ERROR_INVALID_DATA) {
        fputs("FAIL: party-transition install accepted a mismatched ShowPartyMembers entry\n",
            stderr);
        ++*failures;
        SudekiMpUninstallZoneTransitionTrace();
    }
    image[RVA_ZONE_SHOW_PARTY_MEMBERS +
        sizeof(party_visibility_entry) - 1u] =
            saved_show_party_members_byte;
    check_zone_patch_probes_restored(
        probes, probe_count, image, failures);

    saved_hide_party_members_byte =
        image[RVA_ZONE_HIDE_PARTY_MEMBERS +
            sizeof(party_visibility_entry) - 1u];
    image[RVA_ZONE_HIDE_PARTY_MEMBERS +
        sizeof(party_visibility_entry) - 1u] ^= 0x01u;
    SetLastError(ERROR_SUCCESS);
    if (SudekiMpInstallZoneTransitionTrace((HMODULE)image, TRUE, FALSE) ||
        GetLastError() != ERROR_INVALID_DATA) {
        fputs("FAIL: party-transition install accepted a mismatched HidePartyMembers entry\n",
            stderr);
        ++*failures;
        SudekiMpUninstallZoneTransitionTrace();
    }
    image[RVA_ZONE_HIDE_PARTY_MEMBERS +
        sizeof(party_visibility_entry) - 1u] =
            saved_hide_party_members_byte;
    check_zone_patch_probes_restored(
        probes, probe_count, image, failures);

    memcpy(saved_call, image + RVA_ZONE_EXIT_LEAD_MOVE_CALL,
        sizeof(saved_call));
    point_relative_call(image + RVA_ZONE_EXIT_LEAD_MOVE_CALL,
        image + RVA_ZONE_EXIT_LEAD_MOVE + 1u);
    SetLastError(ERROR_SUCCESS);
    if (SudekiMpInstallZoneTransitionTrace((HMODULE)image, TRUE, FALSE) ||
        GetLastError() != ERROR_INVALID_DATA) {
        fputs("FAIL: party-transition install accepted a mismatched exit callsite\n",
            stderr);
        ++*failures;
        SudekiMpUninstallZoneTransitionTrace();
    }
    memcpy(image + RVA_ZONE_EXIT_LEAD_MOVE_CALL, saved_call,
        sizeof(saved_call));
    check_zone_patch_probes_restored(
        probes, probe_count, image, failures);

    if (!SudekiMpInstallZoneTransitionTrace((HMODULE)image, TRUE, FALSE)) {
        fprintf(stderr,
            "FAIL: party-transition exact-image install failed (error=%lu)\n",
            (unsigned long)GetLastError());
        ++*failures;
        return;
    }
    for (index = 0u; index + 2u < probe_count; ++index) {
        if (image[probes[index].rva] != 0xe9u) {
            fprintf(stderr, "FAIL: party-transition hook not installed: %s\n",
                probes[index].name);
            ++*failures;
        }
    }
    for (index = 5u; index < sizeof(party_visibility_entry); ++index) {
        if (image[RVA_ZONE_HIDE_PARTY_MEMBERS + index] != 0x90u) {
            fputs("FAIL: HidePartyMembers inline hook did not cover its exact entry\n",
                stderr);
            ++*failures;
            break;
        }
    }
    if (relative_call_target(image + RVA_ZONE_ENTER_LEAD_POP_CALL) ==
            image + RVA_ZONE_POP_TO_NAMED_LOCATION ||
        relative_call_target(image + RVA_ZONE_EXIT_LEAD_MOVE_CALL) ==
            image + RVA_ZONE_EXIT_LEAD_MOVE) {
        fputs("FAIL: party-transition placement callsites were not redirected\n",
            stderr);
        ++*failures;
    }
    SudekiMpUninstallZoneTransitionTrace();
    check_zone_patch_probes_restored(
        probes, probe_count, image, failures);
}

int wmain(int argc, wchar_t **argv) {
    uint8_t *file;
    uint8_t *image;
    DWORD file_size;
    size_t index;
    int failures = 0;
    unsigned int quick_menu_isolation_state;
    BOOL shared_modal_recovery_pending;
    unsigned int roster_player_one_type;
    unsigned int roster_player_two_type;
    uint8_t minimap_snapshot_call_original[5];
    const UINT second_player_skill_keys[4] = {
        VK_F1, VK_F2, VK_F3, VK_F4
    };
    static const uint8_t set_render_camera_original[] = {
        0x55, 0x8b, 0xec, 0x83, 0xe4, 0xf8
    };

    if (argc != 2) {
        fwprintf(stderr, L"usage: SudekiMP.SkillTraceImageTest.exe SUDEKI.exe\n");
        return 2;
    }
    file = read_file(argv[1], &file_size);
    if (file == NULL) {
        fwprintf(stderr, L"failed to read PE image (error=%lu)\n",
            (unsigned long)GetLastError());
        return 1;
    }
    image = map_pe_image(file, file_size);
    HeapFree(GetProcessHeap(), 0, file);
    if (image == NULL) {
        fputs("failed to map PE image\n", stderr);
        return 1;
    }
    memcpy(
        minimap_snapshot_call_original,
        image + RVA_MINIMAP_SNAPSHOT_POINTER_CALL,
        sizeof(minimap_snapshot_call_original)
    );
    if (SudekiMpSplitScreenSharedInteractionModalActive()) {
        fputs("FAIL: uninstalled shared modal inspector quiesced gameplay\n",
            stderr);
        ++failures;
    }

    {
        static const uint8_t message_pump_accelerator_call[] = {
            0x6a, 0x65,
            0x6a, 0x00,
            0xff, 0x15, 0xec, 0xa0, 0x69, 0x00,
            0x50,
            0xff, 0x15, 0xf0, 0xa1, 0x69, 0x00
        };
        if (memcmp(
                image + SUDEKIMP_MESSAGE_PUMP_ACCELERATOR_CALL_RVA,
                message_pump_accelerator_call,
                sizeof(message_pump_accelerator_call)) != 0 ||
            *(const uint32_t *)(
                image + SUDEKIMP_LOAD_ACCELERATORS_IAT_RVA) !=
                SUDEKIMP_LOAD_ACCELERATORS_IMPORT_NAME_RVA) {
            fputs("FAIL: exact accelerator-cache import seam mismatch\n",
                stderr);
            ++failures;
        }
    }

    if (SudekiMpSplitScreenQuickMenuLiveViewAccepted(
            FALSE, TRUE, FALSE, FALSE) ||
        SudekiMpSplitScreenQuickMenuLiveViewAccepted(
            TRUE, FALSE, FALSE, FALSE) ||
        !SudekiMpSplitScreenQuickMenuLiveViewAccepted(
            TRUE, TRUE, FALSE, FALSE) ||
        !SudekiMpSplitScreenQuickMenuLiveViewAccepted(
            TRUE, TRUE, TRUE, TRUE) ||
        SudekiMpSplitScreenQuickMenuLiveViewAccepted(
            TRUE, TRUE, TRUE, FALSE)) {
        fputs("FAIL: Quick Menu live-view acceptance truth table mismatch\n",
            stderr);
        ++failures;
    }

    if (SudekiMpSplitScreenClassifySharedInteractionModal(
            FALSE, FALSE, FALSE, 0u, 0u) !=
                SUDEKIMP_SHARED_INTERACTION_MODAL_UNCERTAIN ||
        SudekiMpSplitScreenClassifySharedInteractionModal(
            TRUE, FALSE, FALSE, 0u, 0u) !=
                SUDEKIMP_SHARED_INTERACTION_MODAL_NONE ||
        SudekiMpSplitScreenClassifySharedInteractionModal(
            TRUE, TRUE, FALSE, 0u, 0u) !=
                SUDEKIMP_SHARED_INTERACTION_MODAL_SHOP ||
        SudekiMpSplitScreenClassifySharedInteractionModal(
            TRUE, FALSE, TRUE, 0u, 0u) !=
                SUDEKIMP_SHARED_INTERACTION_MODAL_BLACKSMITH ||
        SudekiMpSplitScreenClassifySharedInteractionModal(
            TRUE, FALSE, FALSE, 7u, 0u) !=
                SUDEKIMP_SHARED_INTERACTION_MODAL_SHOP ||
        SudekiMpSplitScreenClassifySharedInteractionModal(
            TRUE, FALSE, FALSE, 0u, 8u) !=
                SUDEKIMP_SHARED_INTERACTION_MODAL_SHOP ||
        SudekiMpSplitScreenClassifySharedInteractionModal(
            TRUE, FALSE, FALSE, 9u, 0u) !=
                SUDEKIMP_SHARED_INTERACTION_MODAL_SHOP ||
        SudekiMpSplitScreenClassifySharedInteractionModal(
            TRUE, FALSE, FALSE, 0u, 9u) !=
                SUDEKIMP_SHARED_INTERACTION_MODAL_SHOP ||
        SudekiMpSplitScreenClassifySharedInteractionModal(
            TRUE, FALSE, FALSE, 5u, 0u) !=
                SUDEKIMP_SHARED_INTERACTION_MODAL_BLACKSMITH ||
        SudekiMpSplitScreenClassifySharedInteractionModal(
            TRUE, FALSE, FALSE, 0u, 6u) !=
                SUDEKIMP_SHARED_INTERACTION_MODAL_BLACKSMITH ||
        SudekiMpSplitScreenClassifySharedInteractionModal(
            TRUE, TRUE, TRUE, 0u, 0u) !=
                SUDEKIMP_SHARED_INTERACTION_MODAL_UNCERTAIN) {
        fputs("FAIL: shared Shop/Blacksmith full-width policy mismatch\n",
            stderr);
        ++failures;
    }
    if (SudekiMpSplitScreenNativeBlacksmithReportedActive(
            TRUE, TRUE, FALSE, 0u, 0u) ||
        !SudekiMpSplitScreenNativeBlacksmithReportedActive(
            TRUE, FALSE, FALSE, 0u, 0u) ||
        !SudekiMpSplitScreenNativeBlacksmithReportedActive(
            TRUE, TRUE, TRUE, 0u, 0u) ||
        !SudekiMpSplitScreenNativeBlacksmithReportedActive(
            TRUE, TRUE, FALSE, 5u, 0u) ||
        !SudekiMpSplitScreenNativeBlacksmithReportedActive(
            TRUE, TRUE, FALSE, 0u, 6u)) {
        fputs("FAIL: mod-owned Blacksmith native-layer exclusion policy mismatch\n",
            stderr);
        ++failures;
    }
    if (SudekiMpSplitScreenSharedInteractionModalShouldQuiesce(
            FALSE,
            SUDEKIMP_SHARED_INTERACTION_MODAL_UNCERTAIN,
            TRUE) ||
        !SudekiMpSplitScreenSharedInteractionModalShouldQuiesce(
            TRUE,
            SUDEKIMP_SHARED_INTERACTION_MODAL_UNCERTAIN,
            FALSE) ||
        SudekiMpSplitScreenSharedInteractionModalShouldQuiesce(
            TRUE,
            SUDEKIMP_SHARED_INTERACTION_MODAL_NONE,
            FALSE) ||
        !SudekiMpSplitScreenSharedInteractionModalShouldQuiesce(
            TRUE,
            SUDEKIMP_SHARED_INTERACTION_MODAL_NONE,
            TRUE)) {
        fputs("FAIL: installed/uninstalled shared modal quiesce policy mismatch\n",
            stderr);
        ++failures;
    }
    if (!SudekiMpSplitScreenSharedInteractionRecoveryEligible(
            TRUE, TRUE, TRUE, TRUE, TRUE, TRUE, TRUE) ||
        SudekiMpSplitScreenSharedInteractionRecoveryEligible(
            FALSE, TRUE, TRUE, TRUE, TRUE, TRUE, TRUE) ||
        SudekiMpSplitScreenSharedInteractionRecoveryEligible(
            TRUE, FALSE, TRUE, TRUE, TRUE, TRUE, TRUE) ||
        SudekiMpSplitScreenSharedInteractionRecoveryEligible(
            TRUE, TRUE, FALSE, TRUE, TRUE, TRUE, TRUE) ||
        SudekiMpSplitScreenSharedInteractionRecoveryEligible(
            TRUE, TRUE, TRUE, FALSE, TRUE, TRUE, TRUE) ||
        SudekiMpSplitScreenSharedInteractionRecoveryEligible(
            TRUE, TRUE, TRUE, TRUE, FALSE, TRUE, TRUE) ||
        SudekiMpSplitScreenSharedInteractionRecoveryEligible(
            TRUE, TRUE, TRUE, TRUE, TRUE, FALSE, TRUE) ||
        SudekiMpSplitScreenSharedInteractionRecoveryEligible(
            TRUE, TRUE, TRUE, TRUE, TRUE, TRUE, FALSE)) {
        fputs("FAIL: shared modal live split recovery eligibility mismatch\n",
            stderr);
        ++failures;
    }
    shared_modal_recovery_pending =
        SudekiMpSplitScreenSharedInteractionRecoveryPendingNext(
            FALSE, TRUE, TRUE, TRUE, FALSE);
    if (!shared_modal_recovery_pending) {
        fputs("FAIL: eligible modal close did not arm cache recovery\n",
            stderr);
        ++failures;
    }
    shared_modal_recovery_pending =
        SudekiMpSplitScreenSharedInteractionRecoveryPendingNext(
            shared_modal_recovery_pending,
            FALSE,
            FALSE,
            FALSE,
            FALSE
        );
    if (shared_modal_recovery_pending) {
        fputs("FAIL: Player 2 dropout did not cancel cache recovery\n",
            stderr);
        ++failures;
    }
    shared_modal_recovery_pending =
        SudekiMpSplitScreenSharedInteractionRecoveryPendingNext(
            shared_modal_recovery_pending,
            FALSE,
            FALSE,
            TRUE,
            FALSE
        );
    if (shared_modal_recovery_pending) {
        fputs("FAIL: Player 2 rejoin rearmed a cancelled modal recovery\n",
            stderr);
        ++failures;
    }
    if (SudekiMpSplitScreenSharedInteractionRecoveryPendingNext(
            TRUE, FALSE, FALSE, TRUE, TRUE)) {
        fputs("FAIL: fresh split cache pair did not complete modal recovery\n",
            stderr);
        ++failures;
    }

    if (SudekiMpSplitScreenMinimapOwnerIsPlayerTwo(
            FALSE, TRUE, TRUE) ||
        SudekiMpSplitScreenMinimapOwnerIsPlayerTwo(
            TRUE, FALSE, TRUE) ||
        SudekiMpSplitScreenMinimapOwnerIsPlayerTwo(
            TRUE, TRUE, FALSE) ||
        !SudekiMpSplitScreenMinimapOwnerIsPlayerTwo(
            TRUE, TRUE, TRUE)) {
        fputs("FAIL: minimap update-owner latch policy mismatch\n",
            stderr);
        ++failures;
    }
    if (!SudekiMpSplitScreenMinimapCaptureAllowed(
            FALSE, FALSE, TRUE, FALSE) ||
        SudekiMpSplitScreenMinimapCaptureAllowed(
            TRUE, FALSE, FALSE, FALSE) ||
        SudekiMpSplitScreenMinimapCaptureAllowed(
            TRUE, FALSE, FALSE, TRUE) ||
        !SudekiMpSplitScreenMinimapCaptureAllowed(
            TRUE, TRUE, FALSE, FALSE) ||
        !SudekiMpSplitScreenMinimapCaptureAllowed(
            TRUE, TRUE, TRUE, TRUE) ||
        SudekiMpSplitScreenMinimapCaptureAllowed(
            TRUE, TRUE, FALSE, TRUE) ||
        SudekiMpSplitScreenMinimapCaptureAllowed(
            TRUE, TRUE, TRUE, FALSE)) {
        fputs("FAIL: minimap cache owner-match policy mismatch\n", stderr);
        ++failures;
    }

    {
        unsigned short native_generation_baseline = 7u;
        void *native_camera_entity = &native_generation_baseline;
        void *native_camera_party_slot[3] = {
            native_camera_entity,
            NULL,
            NULL
        };

        if (SudekiMpSplitScreenObserveNativeCameraGeneration(
                &native_generation_baseline, 8u, TRUE) ||
            native_generation_baseline != 8u ||
            SudekiMpSplitScreenObserveNativeCameraGeneration(
                &native_generation_baseline, 8u, FALSE) ||
            !SudekiMpSplitScreenObserveNativeCameraGeneration(
                &native_generation_baseline, 9u, FALSE)) {
            fputs("FAIL: manual P2 camera generation write falsely proved native readiness\n",
                stderr);
            ++failures;
        }
        if (SudekiMpSplitScreenNativeCameraTargetFromPartySlot(NULL) != NULL ||
            SudekiMpSplitScreenNativeCameraTargetFromPartySlot(
                native_camera_party_slot) != native_camera_entity ||
            SudekiMpSplitScreenNativeCameraTargetFromPartySlot(
                native_camera_party_slot) == native_camera_party_slot) {
            fputs("FAIL: native P2 camera target resolved to the intrusive party slot instead of its entity\n",
                stderr);
            ++failures;
        }
    }

    if (SudekiMpSplitScreenTemporaryCameraPolicy(
            FALSE, -1, FALSE) != SUDEKIMP_TEMP_CAMERA_OUTSIDE ||
        SudekiMpSplitScreenTemporaryCameraPolicy(
            TRUE, -1, FALSE) !=
                SUDEKIMP_TEMP_CAMERA_SHARED_FULL_WIDTH ||
        SudekiMpSplitScreenTemporaryCameraPolicy(
            TRUE, 1, TRUE) !=
                SUDEKIMP_TEMP_CAMERA_SHARED_FULL_WIDTH ||
        SudekiMpSplitScreenTemporaryCameraPolicy(
            TRUE, 0, FALSE) !=
                SUDEKIMP_TEMP_CAMERA_SHARED_FULL_WIDTH ||
        SudekiMpSplitScreenTemporaryCameraPolicy(
            TRUE, 0, TRUE) !=
                SUDEKIMP_TEMP_CAMERA_NATIVE_PLAYER_TWO) {
        fputs("FAIL: settled TEMP camera policy did not fail closed outside proven native Exploration\n",
            stderr);
        ++failures;
    }

    {
        const void *player_one = (const void *)(uintptr_t)0x11110000u;
        const void *player_two = (const void *)(uintptr_t)0x22220000u;

        if (!SudekiMpSplitScreenRosterLeadReady(
                player_one, player_one, player_one,
                1u, 1u, 0u, 1u, 0u, 0u, 0u) ||
            SudekiMpSplitScreenRosterLeadReady(
                player_two, player_one, player_one,
                1u, 1u, 0u, 1u, 0u, 0u, 0u) ||
            SudekiMpSplitScreenRosterLeadReady(
                player_one, player_two, player_one,
                1u, 1u, 0u, 1u, 0u, 0u, 0u) ||
            SudekiMpSplitScreenRosterLeadReady(
                player_one, player_one, player_one,
                0u, 0u, 0u, 1u, 0u, 0u, 0u) ||
            SudekiMpSplitScreenRosterLeadReady(
                player_one, player_one, player_one,
                1u, 1u, 1u, 1u, 0u, 0u, 0u) ||
            SudekiMpSplitScreenRosterLeadReady(
                player_one, player_one, player_one,
                1u, 1u, 0u, 0u, 0u, 0u, 0u) ||
            SudekiMpSplitScreenRosterLeadReady(
                player_one, player_one, player_one,
                1u, 1u, 0u, 1u, 1u, 0u, 0u) ||
            SudekiMpSplitScreenRosterLeadReady(
                player_one, player_one, player_one,
                1u, 1u, 0u, 1u, 0u, 1u, 0u) ||
            SudekiMpSplitScreenRosterLeadReady(
                player_one, player_one, player_one,
                1u, 1u, 0u, 1u, 0u, 0u, 1u)) {
            fputs("FAIL: co-op roster lead-readiness truth table mismatch\n",
                stderr);
            ++failures;
        }
        if (!SudekiMpSplitScreenRosterLockHealthy(
                player_one, player_one, player_one,
                player_two, player_two) ||
            SudekiMpSplitScreenRosterLockHealthy(
                player_two, player_one, player_one,
                player_two, player_two) ||
            SudekiMpSplitScreenRosterLockHealthy(
                player_one, player_two, player_one,
                player_two, player_two) ||
            SudekiMpSplitScreenRosterLockHealthy(
                player_one, player_one, player_one,
                player_one, player_two)) {
            fputs("FAIL: co-op roster lock-health truth table mismatch\n",
                stderr);
            ++failures;
        }
    }

    quick_menu_isolation_state =
        SudekiMpSplitScreenQuickMenuIsolationBeginState(
            SUDEKIMP_QUICK_MENU_ISOLATION_IDLE,
            FALSE,
            TRUE,
            TRUE
        );
    if (quick_menu_isolation_state !=
            SUDEKIMP_QUICK_MENU_ISOLATION_ACTIVE ||
        SudekiMpSplitScreenQuickMenuSubmitShouldBeSuppressed(
            TRUE, TRUE, TRUE, TRUE) ||
        !SudekiMpSplitScreenQuickMenuSubmitShouldBeSuppressed(
            TRUE, TRUE, FALSE, FALSE) ||
        SudekiMpSplitScreenQuickMenuSubmitShouldBeSuppressed(
            TRUE, FALSE, FALSE, FALSE)) {
        fputs("FAIL: Quick Menu P2-first submit cadence mismatch\n", stderr);
        ++failures;
    }
    quick_menu_isolation_state =
        SudekiMpSplitScreenQuickMenuIsolationBeginState(
            SUDEKIMP_QUICK_MENU_ISOLATION_IDLE,
            FALSE,
            TRUE,
            FALSE
        );
    quick_menu_isolation_state =
        SudekiMpSplitScreenQuickMenuIsolationBeginState(
            quick_menu_isolation_state,
            TRUE,
            TRUE,
            TRUE
        );
    if (quick_menu_isolation_state !=
        SUDEKIMP_QUICK_MENU_ISOLATION_FAILED) {
        fputs("FAIL: Quick Menu isolation upgraded in the middle of an open menu\n",
            stderr);
        ++failures;
    }
    quick_menu_isolation_state =
        SudekiMpSplitScreenQuickMenuIsolationBeginState(
            SUDEKIMP_QUICK_MENU_ISOLATION_ACTIVE,
            TRUE,
            FALSE,
            FALSE
        );
    quick_menu_isolation_state =
        SudekiMpSplitScreenQuickMenuIsolationEndState(
            quick_menu_isolation_state,
            TRUE
        );
    if (quick_menu_isolation_state !=
        SUDEKIMP_QUICK_MENU_ISOLATION_TAIL) {
        fputs("FAIL: Quick Menu close tail did not preserve a queued submit\n",
            stderr);
        ++failures;
    }
    quick_menu_isolation_state =
        SudekiMpSplitScreenQuickMenuIsolationEndState(
            quick_menu_isolation_state,
            FALSE
        );
    if (quick_menu_isolation_state !=
            SUDEKIMP_QUICK_MENU_ISOLATION_IDLE ||
        SudekiMpSplitScreenQuickMenuIsolationCancelState(TRUE) !=
            SUDEKIMP_QUICK_MENU_ISOLATION_FAILED ||
        SudekiMpSplitScreenQuickMenuIsolationCancelState(FALSE) !=
            SUDEKIMP_QUICK_MENU_ISOLATION_IDLE) {
        fputs("FAIL: Quick Menu close/failure/quit state sequence mismatch\n",
            stderr);
        ++failures;
    }

    if (*(const uint32_t *)(
            image + RVA_QUICK_MENU_RENDER_SUBMIT_VTABLE_SLOT) !=
        0x0049bba0u) {
        fputs("FAIL: exact Quick Menu render-submit vtable slot mismatch\n",
            stderr);
        ++failures;
    }
    {
        static const uint8_t shop_active_tail[] = {
            0x85, 0xc0, 0x74, 0x1e, 0x83, 0xb8, 0xb8, 0x00,
            0x00, 0x00, 0x07, 0x74, 0x0f, 0x8b, 0x40, 0x74
        };
        static const uint8_t blacksmith_active_tail[] = {
            0x85, 0xc0, 0x74, 0x04, 0x8a, 0x40, 0x29, 0xc3,
            0x32, 0xc0, 0xc3
        };

        if (image[RVA_SHOP_IS_ACTIVE] != 0xa1u ||
            *(const uint32_t *)(image + RVA_SHOP_IS_ACTIVE + 1u) !=
                0x007c2f88u ||
            memcmp(
                image + RVA_SHOP_IS_ACTIVE + 5u,
                shop_active_tail,
                sizeof(shop_active_tail)) != 0 ||
            image[RVA_BLACKSMITH_IS_ACTIVE] != 0xa1u ||
            *(const uint32_t *)(
                image + RVA_BLACKSMITH_IS_ACTIVE + 1u) != 0x007c2f74u ||
            memcmp(
                image + RVA_BLACKSMITH_IS_ACTIVE + 5u,
                blacksmith_active_tail,
                sizeof(blacksmith_active_tail)) != 0 ||
            *(const uint32_t *)(
                image + RVA_INGAME_UI_CONTROLLER_VTABLE + 0x08u) !=
                    0x0049d1d0u ||
            *(const uint32_t *)(
                image + RVA_INGAME_UI_CONTROLLER_VTABLE + 0x0cu) !=
                    0x0049d8d0u ||
            *(const uint32_t *)(
                image + RVA_INGAME_UI_CONTROLLER_VTABLE + 0x2cu) !=
                    0x0049c930u ||
            *(const uint32_t *)(image + RVA_SHOP_LAYER_VTABLE + 0x08u) !=
                0x00489660u ||
            *(const uint32_t *)(image + RVA_SHOP_LAYER_VTABLE + 0x0cu) !=
                0x0048a210u ||
            *(const uint32_t *)(image + RVA_SHOP_LAYER_VTABLE + 0x2cu) !=
                0x004898a0u ||
            *(const uint32_t *)(image + RVA_SHOP_LAYER_VTABLE + 0x48u) !=
                0x0048c850u ||
            *(const uint32_t *)(image + RVA_SHOP_LAYER_VTABLE + 0x4cu) !=
                0x0048d030u ||
            *(const uint32_t *)(
                image + RVA_BLACKSMITH_LAYER_VTABLE + 0x08u) !=
                    0x0048d6f0u ||
            *(const uint32_t *)(
                image + RVA_BLACKSMITH_LAYER_VTABLE + 0x0cu) !=
                    0x0048e910u ||
            *(const uint32_t *)(
                image + RVA_BLACKSMITH_LAYER_VTABLE + 0x2cu) !=
                    0x0048d970u ||
            *(const uint32_t *)(
                image + RVA_BLACKSMITH_LAYER_VTABLE + 0x48u) !=
                    0x00490c20u ||
            *(const uint32_t *)(
                image + RVA_BLACKSMITH_LAYER_VTABLE + 0x4cu) !=
                    0x00491b40u) {
            fputs("FAIL: exact shared Shop/Blacksmith modal seams mismatch\n",
                stderr);
            ++failures;
        }
    }
    if (relative_call_target(image + RVA_MINIMAP_UPDATE_POINTER_CALL) !=
            image + RVA_HUD_PARTY_POINTER_COPY ||
        relative_call_target(image + RVA_MINIMAP_SNAPSHOT_POINTER_CALL) !=
            image + RVA_HUD_PARTY_POINTER_COPY ||
        relative_call_target(image + RVA_MINIMAP_RENDER_POINTER_CALL) !=
            image + RVA_HUD_PARTY_POINTER_COPY) {
        fputs("FAIL: exact minimap party-owner call seam mismatch\n", stderr);
        ++failures;
    }

    /* Simulate the loader relocation for the absolute jump-table pointer. */
    *(void **)(image + RVA_SCRIPT_CALL_OPCODE_SLOT) =
        image + RVA_SCRIPT_CALL_OPCODE;
    *(void **)(image + RVA_SCRIPT_METHOD_OPCODE_SLOT) =
        image + RVA_SCRIPT_METHOD_OPCODE;
    *(void **)(image + RVA_CHARACTER_INPUT_VTABLE_SLOT) =
        image + RVA_CHARACTER_INPUT_HANDLER;
    *(void **)(image + RVA_CONTROLLER_UPDATE_VTABLE_SLOT) =
        image + RVA_CONTROLLER_UPDATE;
    *(void **)(image + RVA_CAMERA_INPUT_EVENT_VTABLE_SLOT) =
        image + RVA_CAMERA_INPUT_EVENT;
    *(void **)(image + RVA_MOTION_BLUR_POST_RENDER_VTABLE_SLOT) =
        image + RVA_MOTION_BLUR_POST_RENDER;
    *(void **)(image + RVA_SCREENSHOT_POST_RENDER_VTABLE_SLOT) =
        image + RVA_SCREENSHOT_POST_RENDER;
    *(void **)(image + RVA_QUICK_MENU_RENDER_SUBMIT_VTABLE_SLOT) =
        image + RVA_QUICK_MENU_RENDER_SUBMIT;
    *(void **)(image + RVA_INGAME_UI_CONTROLLER_VTABLE + 0x08u) =
        image + RVA_INGAME_UI_CONTROLLER_UPDATE;
    *(void **)(image + RVA_INGAME_UI_CONTROLLER_VTABLE + 0x0cu) =
        image + RVA_INGAME_UI_CONTROLLER_RENDER;
    *(void **)(image + RVA_INGAME_UI_CONTROLLER_VTABLE + 0x2cu) =
        image + RVA_INGAME_UI_CONTROLLER_INPUT;
    *(void **)(image + RVA_SHOP_LAYER_VTABLE + 0x08u) =
        image + RVA_SHOP_LAYER_UPDATE;
    *(void **)(image + RVA_SHOP_LAYER_VTABLE + 0x0cu) =
        image + RVA_SHOP_LAYER_RENDER;
    *(void **)(image + RVA_SHOP_LAYER_VTABLE + 0x2cu) =
        image + RVA_SHOP_LAYER_INPUT;
    *(void **)(image + RVA_SHOP_LAYER_VTABLE + 0x48u) =
        image + RVA_SHOP_LAYER_RESOURCE_CREATE;
    *(void **)(image + RVA_SHOP_LAYER_VTABLE + 0x4cu) =
        image + RVA_SHOP_LAYER_RESOURCE_DESTROY;
    *(void **)(image + RVA_BLACKSMITH_LAYER_VTABLE + 0x08u) =
        image + RVA_BLACKSMITH_LAYER_UPDATE;
    *(void **)(image + RVA_BLACKSMITH_LAYER_VTABLE + 0x0cu) =
        image + RVA_BLACKSMITH_LAYER_RENDER;
    *(void **)(image + RVA_BLACKSMITH_LAYER_VTABLE + 0x2cu) =
        image + RVA_BLACKSMITH_LAYER_INPUT;
    *(void **)(image + RVA_BLACKSMITH_LAYER_VTABLE + 0x48u) =
        image + RVA_BLACKSMITH_LAYER_RESOURCE_CREATE;
    *(void **)(image + RVA_BLACKSMITH_LAYER_VTABLE + 0x4cu) =
        image + RVA_BLACKSMITH_LAYER_RESOURCE_DESTROY;
    *(void **)(image + RVA_ANIMATION_RENDERER_VTABLE + 0x40u) =
        image + RVA_ANIMATION_RENDERER_LOOKUP;
    *(void **)(image + RVA_ANIMATION_RENDERER_VTABLE + 0xf8u) =
        image + RVA_ANIMATION_RENDERER_COUNT;
    *(void **)(image + RVA_ANIMATION_RENDERER_VTABLE + 0xfcu) =
        image + RVA_ANIMATION_RENDERER_SELECTOR_SET;
    *(void **)(image + RVA_ANIMATION_RENDERER_VTABLE + 0x100u) =
        image + RVA_ANIMATION_RENDERER_SELECTOR_GET;
    *(void **)(image + RVA_ANIMATION_RENDERER_VTABLE + 0x104u) =
        image + RVA_ANIMATION_RENDERER_RATE_SET;
    *(void **)(image + RVA_ANIMATION_RENDERER_VTABLE + 0x108u) =
        image + RVA_ANIMATION_RENDERER_RATE_GET;
    *(void **)(image + RVA_ANIMATION_RENDERER_VTABLE + 0x10cu) =
        image + RVA_ANIMATION_RENDERER_TIME_SET;
    *(void **)(image + RVA_ANIMATION_RENDERER_VTABLE + 0x110u) =
        image + RVA_ANIMATION_RENDERER_TIME_GET;
    *(void **)(image + RVA_ANIMATION_RENDERER_VTABLE + 0x114u) =
        image + RVA_ANIMATION_RENDERER_STATE_SET;
    *(void **)(image + RVA_ANIMATION_RENDERER_VTABLE + 0x118u) =
        image + RVA_ANIMATION_RENDERER_STATE_GET;
    *(void **)(image + RVA_ANIMATION_RENDERER_VTABLE + 0x144u) =
        image + RVA_ANIMATION_RENDERER_BLEND_SET;
    *(void **)(image + RVA_ANIMATION_RENDERER_VTABLE + 0x148u) =
        image + RVA_ANIMATION_RENDERER_BLEND_GET;
    *(uint32_t *)(image + RVA_PC_QUIT_SCREEN_SHOW + 3u) =
        (uint32_t)(uintptr_t)(image + RVA_PC_QUIT_SCREEN_GLOBAL);
    *(uint32_t *)(image + RVA_QUICK_MENU_IS_ACTIVE + 1u) =
        (uint32_t)(uintptr_t)(image + RVA_QUICK_MENU_GLOBAL);
    *(uint32_t *)(image + RVA_SHOP_IS_ACTIVE + 1u) =
        (uint32_t)(uintptr_t)(image + RVA_INGAME_UI_CONTROLLER_GLOBAL);
    *(uint32_t *)(image + RVA_BLACKSMITH_START + 1u) =
        (uint32_t)(uintptr_t)(image + RVA_WORLD_SCENE_GLOBAL);
    *(uint32_t *)(image + RVA_BLACKSMITH_IS_ACTIVE + 1u) =
        (uint32_t)(uintptr_t)(image + RVA_BLACKSMITH_LAYER_GLOBAL);
    *(uint32_t *)(image + RVA_CHARACTER_TYPE_TO_PORTRAIT_ENUM + 9u) =
        (uint32_t)(uintptr_t)(image + RVA_CHARACTER_TYPE_TO_PORTRAIT_LOOKUP);
    *(uint32_t *)(image + RVA_HUD_PORTRAIT_RESOURCE_SELECT + 8u) =
        (uint32_t)(uintptr_t)(image + RVA_UI_RESOURCE_TABLE_INITIALIZED);
    *(uint32_t *)(image + RVA_SKILL_DATA_AVAILABLE + 2u) =
        (uint32_t)(uintptr_t)(image + RVA_SKILL_AVAILABILITY_FLAG);
    *(uint32_t *)(image + RVA_SKILL_VALIDATE + 2u) =
        (uint32_t)(uintptr_t)(image + RVA_SKILL_VALIDATE_FLAG);

    check_blacksmith_ui_adapter_exact_image(image, &failures);
    check_blacksmith_roster_actor_identity_policy(&failures);

    {
        uint8_t saved_blacksmith_signature =
            image[RVA_BLACKSMITH_IS_ACTIVE + 5u];

        image[RVA_BLACKSMITH_IS_ACTIVE + 5u] ^= 0xffu;
        SetLastError(ERROR_SUCCESS);
        if (SudekiMpInstallSplitScreenRender(
                (HMODULE)image,
                FALSE,
                FALSE,
                0u,
                FALSE,
                FALSE,
                FALSE,
                FALSE,
                FALSE,
                0.20f,
                2.25f,
                1.50f,
                0.65f)) {
            fputs("FAIL: modal inspector accepted a mismatched exact signature\n",
                stderr);
            ++failures;
            SudekiMpUninstallSplitScreenRender();
        } else if (GetLastError() != ERROR_INVALID_DATA) {
            fprintf(stderr,
                "FAIL: modal inspector signature mismatch returned error=%lu\n",
                (unsigned long)GetLastError());
            ++failures;
        }
        if (SudekiMpSplitScreenSharedInteractionModalActive()) {
            fputs("FAIL: failed modal inspector install quiesced gameplay\n",
                stderr);
            ++failures;
        }
        image[RVA_BLACKSMITH_IS_ACTIVE + 5u] =
            saved_blacksmith_signature;
    }
    check_shared_interaction_modal_runtime(image, &failures);

    for (index = 0u;
            index < sizeof(expected_cleanroom_entries) /
                sizeof(expected_cleanroom_entries[0]);
            ++index) {
        if (memcmp(
                image + expected_cleanroom_entries[index].function_rva,
                expected_cleanroom_entries[index].bytes,
                expected_cleanroom_entries[index].byte_count) != 0) {
            fprintf(
                stderr,
                "FAIL: cleanroom native entry mismatch: %s\n",
                expected_cleanroom_entries[index].name
            );
            ++failures;
        }
    }
    test_zone_transition_exact_image(image, &failures);
    if (failures != 0) {
        VirtualFree(image, 0, MEM_RELEASE);
        return 1;
    }

    if (!SudekiMpInstallSkillTrace((HMODULE)image, 1.0f, 1.0f)) {
        fprintf(stderr, "install rejected image (error=%lu)\n",
            (unsigned long)GetLastError());
        VirtualFree(image, 0, MEM_RELEASE);
        return 1;
    }
    if (!SudekiMpInitializeSkillActivationAbi((HMODULE)image)) {
        fprintf(stderr, "skill activation ABI rejected image (error=%lu)\n",
            (unsigned long)GetLastError());
        SudekiMpUninstallSkillTrace();
        VirtualFree(image, 0, MEM_RELEASE);
        return 1;
    }
    if (!SudekiMpInstallQuickSkillInputTrace((HMODULE)image, TRUE, TRUE)) {
        fprintf(stderr, "quick-skill install rejected image (error=%lu)\n",
            (unsigned long)GetLastError());
        SudekiMpUninstallSkillTrace();
        VirtualFree(image, 0, MEM_RELEASE);
        return 1;
    }
    if (!SudekiMpInstallSpiritStrikeInput((HMODULE)image, -1, 1u, 'G')) {
        fprintf(stderr, "Spirit Strike input install rejected image (error=%lu)\n",
            (unsigned long)GetLastError());
        SudekiMpUninstallQuickSkillInputTrace();
        SudekiMpUninstallSkillTrace();
        VirtualFree(image, 0, MEM_RELEASE);
        return 1;
    }
    if (!SudekiMpInstallCharacterSwitchTrace((HMODULE)image, TRUE)) {
        fprintf(stderr, "character-switch trace install rejected image (error=%lu)\n",
            (unsigned long)GetLastError());
        SudekiMpUninstallSpiritStrikeInput();
        SudekiMpUninstallQuickSkillInputTrace();
        SudekiMpUninstallSkillTrace();
        VirtualFree(image, 0, MEM_RELEASE);
        return 1;
    }
    if (!SudekiMpInstallTalosDefenseTrace((HMODULE)image)) {
        fprintf(stderr, "Talos defense trace rejected image (error=%lu)\n",
            (unsigned long)GetLastError());
        SudekiMpUninstallCharacterSwitchTrace();
        SudekiMpUninstallSpiritStrikeInput();
        SudekiMpUninstallQuickSkillInputTrace();
        SudekiMpUninstallSkillTrace();
        VirtualFree(image, 0, MEM_RELEASE);
        return 1;
    }
    if (image[RVA_APPLY_DAMAGE] != 0xe9u ||
        image[RVA_COLLISION_DAMAGE] != 0xe9u) {
        fputs("FAIL: Talos defense inline hooks were not installed\n", stderr);
        ++failures;
    }
    if (!SudekiMpInstallControlSeparation(
            (HMODULE)image,
            'J',
            TRUE,
            TRUE,
            TRUE,
            10.0f,
            TRUE,
            'U',
            TRUE,
            second_player_skill_keys,
            TRUE,
            TRUE,
            FALSE,
            0.20f)) {
        fprintf(stderr, "control-separation install rejected image (error=%lu)\n",
            (unsigned long)GetLastError());
        SudekiMpUninstallCharacterSwitchTrace();
        SudekiMpUninstallSpiritStrikeInput();
        SudekiMpUninstallQuickSkillInputTrace();
        SudekiMpUninstallSkillTrace();
        VirtualFree(image, 0, MEM_RELEASE);
        return 1;
    }
    if (relative_call_target(image + RVA_PLAYER_MOVE_CALL_ALTERNATE) ==
            image + RVA_ARBITER_MOVEMENT ||
        relative_call_target(image + RVA_PLAYER_MOVE_CALL_NORMAL) ==
            image + RVA_ARBITER_MOVEMENT) {
        fputs("FAIL: roaming boundary did not hook both native Player 1 movement submissions\n",
            stderr);
        ++failures;
    }
    SudekiMpUninstallControlSeparation();
    if (relative_call_target(image + RVA_PLAYER_MOVE_CALL_ALTERNATE) !=
            image + RVA_ARBITER_MOVEMENT ||
        relative_call_target(image + RVA_PLAYER_MOVE_CALL_NORMAL) !=
            image + RVA_ARBITER_MOVEMENT) {
        fputs("FAIL: roaming boundary did not restore both native Player 1 movement submissions\n",
            stderr);
        ++failures;
    }
    if (!SudekiMpInstallControlSeparation(
            (HMODULE)image,
            'J',
            TRUE,
            TRUE,
            FALSE,
            10.0f,
            TRUE,
            'U',
            TRUE,
            second_player_skill_keys,
            TRUE,
            TRUE,
            FALSE,
            0.20f)) {
        fprintf(stderr,
            "control-separation coexistence install rejected image (error=%lu)\n",
            (unsigned long)GetLastError());
        SudekiMpUninstallCharacterSwitchTrace();
        SudekiMpUninstallSpiritStrikeInput();
        SudekiMpUninstallQuickSkillInputTrace();
        SudekiMpUninstallSkillTrace();
        VirtualFree(image, 0, MEM_RELEASE);
        return 1;
    }
    {
        const uint32_t character_switch_rvas[] = {
            RVA_GROUP_PLAYERS_PREVIOUS_CHARACTER,
            RVA_GROUP_PLAYERS_NEXT_CHARACTER
        };

        for (index = 0u;
                index < sizeof(character_switch_rvas) /
                    sizeof(character_switch_rvas[0]);
                ++index) {
            uint8_t saved_entry = image[character_switch_rvas[index]];

            image[character_switch_rvas[index]] ^= 0xffu;
            SetLastError(ERROR_SUCCESS);
            if (SudekiMpInstallSplitScreenRender(
                    (HMODULE)image,
                    FALSE,
                    FALSE,
                    0u,
                    FALSE,
                    FALSE,
                    FALSE,
                    FALSE,
                    FALSE,
                    0.20f,
                    2.25f,
                    1.50f,
                    0.65f)) {
                fprintf(stderr,
                    "FAIL: split-screen render accepted mismatched character-switch consumer %lu\n",
                    (unsigned long)index);
                ++failures;
                SudekiMpUninstallSplitScreenRender();
            } else if (GetLastError() != ERROR_INVALID_DATA) {
                fprintf(stderr,
                    "FAIL: character-switch consumer %lu mismatch returned error=%lu\n",
                    (unsigned long)index,
                    (unsigned long)GetLastError());
                ++failures;
            }
            image[character_switch_rvas[index]] = saved_entry;
        }
    }
    *(void **)(image + RVA_QUICK_MENU_RENDER_SUBMIT_VTABLE_SLOT) =
        image + RVA_QUICK_MENU_IS_ACTIVE;
    if (SudekiMpInstallSplitScreenRender(
            (HMODULE)image,
            TRUE,
            TRUE,
            VK_F9,
            TRUE,
            FALSE,
            FALSE,
            TRUE,
            TRUE,
            0.20f,
            2.25f,
            1.50f,
            0.65f)) {
        fputs("FAIL: split-screen render accepted a mismatched Quick Menu render-submit slot\n",
            stderr);
        ++failures;
        SudekiMpUninstallSplitScreenRender();
    }
    *(void **)(image + RVA_QUICK_MENU_RENDER_SUBMIT_VTABLE_SLOT) =
        image + RVA_QUICK_MENU_RENDER_SUBMIT;
    *(void **)(image + RVA_CAMERA_INPUT_EVENT_VTABLE_SLOT) =
        image + RVA_QUICK_MENU_IS_ACTIVE;
    SetLastError(ERROR_SUCCESS);
    if (SudekiMpInstallSplitScreenRender(
            (HMODULE)image,
            TRUE,
            TRUE,
            VK_F9,
            FALSE,
            FALSE,
            TRUE,
            FALSE,
            FALSE,
            0.20f,
            2.25f,
            1.50f,
            0.65f)) {
        fputs("FAIL: split-screen render accepted a mismatched native camera input slot\n",
            stderr);
        ++failures;
        SudekiMpUninstallSplitScreenRender();
    } else if (GetLastError() != ERROR_INVALID_DATA) {
        fprintf(stderr,
            "FAIL: native camera input slot mismatch returned error=%lu\n",
            (unsigned long)GetLastError());
        ++failures;
    }
    *(void **)(image + RVA_CAMERA_INPUT_EVENT_VTABLE_SLOT) =
        image + RVA_CAMERA_INPUT_EVENT;
    {
        int32_t minimap_render_displacement;

        memcpy(
            &minimap_render_displacement,
            image + RVA_MINIMAP_RENDER_POINTER_CALL + 1u,
            sizeof(minimap_render_displacement)
        );
        image[RVA_MINIMAP_RENDER_POINTER_CALL + 1u] ^= 0xffu;
        if (SudekiMpInstallSplitScreenRender(
                (HMODULE)image,
                TRUE,
                TRUE,
                VK_F9,
                TRUE,
                FALSE,
                FALSE,
                TRUE,
                TRUE,
                0.20f,
                2.25f,
                1.50f,
                0.65f)) {
            fputs("FAIL: split-screen render accepted a mismatched minimap render seam\n",
                stderr);
            ++failures;
            SudekiMpUninstallSplitScreenRender();
        }
        if (relative_call_target(image + RVA_MINIMAP_UPDATE_POINTER_CALL) !=
                image + RVA_HUD_PARTY_POINTER_COPY ||
            memcmp(
                image + RVA_MINIMAP_SNAPSHOT_POINTER_CALL,
                minimap_snapshot_call_original,
                sizeof(minimap_snapshot_call_original)) != 0) {
            fputs("FAIL: minimap partial-install rollback retained an earlier hook\n",
                stderr);
            ++failures;
        }
        memcpy(
            image + RVA_MINIMAP_RENDER_POINTER_CALL + 1u,
            &minimap_render_displacement,
            sizeof(minimap_render_displacement)
        );
        if (relative_call_target(image + RVA_MINIMAP_RENDER_POINTER_CALL) !=
            image + RVA_HUD_PARTY_POINTER_COPY) {
            fputs("FAIL: minimap render seam was not restored after mismatch probe\n",
                stderr);
            ++failures;
        }
    }
    if (!SudekiMpSplitScreenSetRosterTypes(0x11u, 0x21u) ||
        !SudekiMpSplitScreenGetRosterTypes(
            &roster_player_one_type, &roster_player_two_type) ||
        roster_player_one_type != 0x11u ||
        roster_player_two_type != 0x21u) {
        fputs("FAIL: preinstall co-op roster contract was not recorded\n",
            stderr);
        ++failures;
    }
    if (!SudekiMpInstallSplitScreenRender(
            (HMODULE)image,
            TRUE,
            TRUE,
            VK_F9,
            TRUE,
            FALSE,
            TRUE,
            TRUE,
            TRUE,
            0.20f,
            2.25f,
            1.50f,
            0.65f)) {
        fprintf(stderr, "split-screen render install rejected image (error=%lu)\n",
            (unsigned long)GetLastError());
        SudekiMpUninstallControlSeparation();
        SudekiMpUninstallCharacterSwitchTrace();
        SudekiMpUninstallSpiritStrikeInput();
        SudekiMpUninstallQuickSkillInputTrace();
        SudekiMpUninstallSkillTrace();
        VirtualFree(image, 0, MEM_RELEASE);
        return 1;
    }
    if (!SudekiMpSplitScreenGetRosterTypes(
            &roster_player_one_type, &roster_player_two_type) ||
        roster_player_one_type != 0x11u ||
        roster_player_two_type != 0x21u) {
        fputs("FAIL: split-screen install discarded the preinstalled co-op roster contract\n",
            stderr);
        ++failures;
    }
    if (!SudekiMpSplitScreenRosterParticipationAvailable() ||
        !SudekiMpSplitScreenRosterParticipationRequested()) {
        fputs("FAIL: installed roster did not begin as available and joined\n",
            stderr);
        ++failures;
    }
    if (!SudekiMpSplitScreenRequestRosterParticipation(FALSE) ||
        SudekiMpSplitScreenRosterParticipationRequested() ||
        SudekiMpSplitScreenRuntimeEnabled()) {
        fputs("FAIL: roster drop-out did not preserve a disabled participation/runtime state\n",
            stderr);
        ++failures;
    }
    if (!SudekiMpSplitScreenRequestRosterParticipation(TRUE) ||
        !SudekiMpSplitScreenRosterParticipationRequested() ||
        !SudekiMpSplitScreenBeginPartyTransition()) {
        fputs("FAIL: roster drop-in or transition quarantine could not begin\n",
            stderr);
        ++failures;
    } else {
        SetLastError(ERROR_SUCCESS);
        if (SudekiMpSplitScreenRequestRosterParticipation(FALSE) ||
            GetLastError() != ERROR_BUSY) {
            fputs("FAIL: roster participation changed inside a party transition\n",
                stderr);
            ++failures;
        }
        SudekiMpSplitScreenEndPartyTransition(TRUE);
        if (!SudekiMpSplitScreenRosterParticipationRequested()) {
            fputs("FAIL: successful party transition discarded the joined state\n",
                stderr);
            ++failures;
        }
    }
    if (!SudekiMpSplitScreenBeginPartyTransition()) {
        fputs("FAIL: second party transition quarantine could not begin\n",
            stderr);
        ++failures;
    } else {
        SudekiMpSplitScreenEndPartyTransition(FALSE);
        if (SudekiMpSplitScreenRosterParticipationRequested()) {
            fputs("FAIL: failed party placement did not leave Player 2 dropped out\n",
                stderr);
            ++failures;
        }
    }
    if (!SudekiMpSplitScreenRequestRosterParticipation(TRUE) ||
        !SudekiMpSplitScreenSetRuntimeEnabled(TRUE)) {
        fputs("FAIL: roster could not request drop-in after transition failure\n",
            stderr);
        ++failures;
    }
    if (!SudekiMpSplitScreenClearRosterTypes() ||
        SudekiMpSplitScreenRosterParticipationAvailable() ||
        SudekiMpSplitScreenRosterParticipationRequested() ||
        SudekiMpSplitScreenRuntimeEnabled() ||
        SudekiMpSplitScreenGetRosterTypes(
            &roster_player_one_type, &roster_player_two_type)) {
        fputs("FAIL: Single Player roster clear retained a multiplayer contract or runtime\n",
            stderr);
        ++failures;
    }
    if (!SudekiMpSplitScreenSetRosterTypes(0x11u, 0x21u) ||
        !SudekiMpSplitScreenSetRuntimeEnabled(TRUE)) {
        fputs("FAIL: co-op roster could not be republished after Single Player clear\n",
            stderr);
        ++failures;
    }
    if (image[RVA_CAMERA_MANAGER_SET_RENDER_CAMERA] != 0xe9) {
        fputs("FAIL: SetRenderCamera inline hook was not installed\n", stderr);
        ++failures;
    }
    if (*(void **)(image + RVA_MOTION_BLUR_POST_RENDER_VTABLE_SLOT) ==
            image + RVA_MOTION_BLUR_POST_RENDER ||
        *(void **)(image + RVA_SCREENSHOT_POST_RENDER_VTABLE_SLOT) ==
            image + RVA_SCREENSHOT_POST_RENDER) {
        fputs("FAIL: Spirit viewport effect callbacks were not redirected\n",
            stderr);
        ++failures;
    }
    if (*(void **)(image + RVA_QUICK_MENU_RENDER_SUBMIT_VTABLE_SLOT) ==
            image + RVA_QUICK_MENU_RENDER_SUBMIT) {
        fputs("FAIL: Quick Menu render-submit vtable slot was not redirected\n",
            stderr);
        ++failures;
    }
    if (!SudekiMpInstallPlayerInputTrace((HMODULE)image)) {
        fprintf(stderr, "player-input trace install rejected image (error=%lu)\n",
            (unsigned long)GetLastError());
        SudekiMpUninstallSplitScreenRender();
        SudekiMpUninstallControlSeparation();
        SudekiMpUninstallCharacterSwitchTrace();
        SudekiMpUninstallSpiritStrikeInput();
        SudekiMpUninstallQuickSkillInputTrace();
        SudekiMpUninstallSkillTrace();
        VirtualFree(image, 0, MEM_RELEASE);
        return 1;
    }
    if (*(void **)(image + RVA_CHARACTER_INPUT_VTABLE_SLOT) ==
        image + RVA_CHARACTER_INPUT_HANDLER) {
        fputs("FAIL: character input vtable slot was not redirected\n", stderr);
        ++failures;
    }
    if (*(void **)(image + RVA_CONTROLLER_UPDATE_VTABLE_SLOT) ==
        image + RVA_CONTROLLER_UPDATE) {
        fputs("FAIL: controller update vtable slot was not redirected\n", stderr);
        ++failures;
    }
    if (relative_call_target(image + RVA_PLAYER_MOVE_CALL_ALTERNATE) ==
            image + RVA_ARBITER_MOVEMENT ||
        relative_call_target(image + RVA_PLAYER_MOVE_CALL_NORMAL) ==
            image + RVA_ARBITER_MOVEMENT) {
        fputs("FAIL: one or more player movement calls were not redirected\n",
            stderr);
        ++failures;
    }
    if (relative_call_target(image + RVA_MAIN_FRAME_UPDATE_CALL) ==
        image + RVA_MAIN_FRAME_UPDATE) {
        fputs("FAIL: main-frame input poll call was not redirected\n", stderr);
        ++failures;
    }
    if (relative_call_target(image + RVA_FRAME_END_CALL_MAIN) ==
            image + RVA_FRAME_END) {
        fputs("FAIL: gameplay-gated frame-end compositor was not redirected\n",
            stderr);
        ++failures;
    }
    if (relative_call_target(image + RVA_RENDER_START_CALL_MAIN) ==
            image + RVA_RENDER_START) {
        fputs("FAIL: render-only camera start call was not redirected\n",
            stderr);
        ++failures;
    }
    if (*(void **)(image + RVA_CAMERA_INPUT_EVENT_VTABLE_SLOT) ==
            image + RVA_CAMERA_INPUT_EVENT) {
        fputs("FAIL: Player 2 native camera input gate was not redirected\n",
            stderr);
        ++failures;
    }
    if (relative_call_target(image + RVA_PC_QUIT_SCREEN_RENDER_CALL) ==
            image + RVA_PC_QUIT_SCREEN_RENDER) {
        fputs("FAIL: pre-Quit cached-backdrop call was not redirected\n",
            stderr);
        ++failures;
    }
    if (relative_call_target(image + RVA_HUD_GROUP_VALUES_POINTER_CALL) ==
            image + RVA_HUD_PARTY_POINTER_COPY ||
        relative_call_target(image + RVA_HUD_GIZMO_VALUES_POINTER_CALL) ==
            image + RVA_HUD_PARTY_POINTER_COPY ||
        relative_call_target(image + RVA_HUD_GIZMO_NAME_POINTER_CALL) ==
            image + RVA_HUD_PARTY_POINTER_COPY ||
        relative_call_target(image + RVA_HUD_GIZMO_STATUS_POINTER_CALL) ==
            image + RVA_HUD_PARTY_POINTER_COPY) {
        fputs("FAIL: one or more viewport HUD ownership calls were not redirected\n",
            stderr);
        ++failures;
    }
    if (relative_call_target(image + RVA_MINIMAP_UPDATE_POINTER_CALL) ==
            image + RVA_HUD_PARTY_POINTER_COPY ||
        relative_call_target(image + RVA_MINIMAP_RENDER_POINTER_CALL) ==
            image + RVA_HUD_PARTY_POINTER_COPY ||
        memcmp(
            image + RVA_MINIMAP_SNAPSHOT_POINTER_CALL,
            minimap_snapshot_call_original,
            sizeof(minimap_snapshot_call_original)) != 0) {
        fputs("FAIL: viewport minimap hooks or native last-cluster snapshot ownership mismatch\n",
            stderr);
        ++failures;
    }
    if (relative_call_target(image + RVA_HUD_GIZMO_PORTRAIT_POINTER_CALL) !=
            image + RVA_HUD_PARTY_POINTER_COPY ||
        relative_call_target(
            image + RVA_HUD_PORTRAIT_RESOURCE_ASSIGNMENT_CALL) !=
            image + RVA_HUD_PORTRAIT_RESOURCE_ASSIGNMENT) {
        fputs("FAIL: one or more native portrait-refresh calls were unexpectedly redirected\n",
            stderr);
        ++failures;
    }
    if (relative_call_target(image + RVA_RENDER_PHASE_CALL_MAIN) !=
            image + RVA_RENDER_PHASE ||
        relative_call_target(image + RVA_RENDER_FIRST_PHASE_CALL_MAIN) !=
            image + RVA_RENDER_FIRST_PHASE) {
        fputs("FAIL: native primary render sequence was redirected\n",
            stderr);
        ++failures;
    }
    if (relative_call_target(image + RVA_RENDER_PHASE_CALL_WORLD_PREPASS) !=
            image + RVA_RENDER_PHASE ||
        relative_call_target(image + RVA_RENDER_PHASE_CALL_WORLD) !=
            image + RVA_RENDER_PHASE ||
        relative_call_target(image + RVA_RENDER_PHASE_CALL_WORLD_OFFSET) !=
            image + RVA_RENDER_PHASE) {
        fputs("FAIL: one or more world subpass calls were redirected\n",
            stderr);
        ++failures;
    }
    if (relative_call_target(image + RVA_UI_SCENE_RENDER_CALL) !=
            image + RVA_UI_SCENE_RENDER) {
        fputs("FAIL: native shared UI queue drain was redirected\n", stderr);
        ++failures;
    }
    if (relative_call_target(image + RVA_QUICK_SKILL_ACTION_CALL) ==
        image + RVA_QUICK_SKILL_ACTION) {
        fputs("FAIL: QuickSkill action call was not redirected\n", stderr);
        ++failures;
    }
    if (relative_call_target(image + RVA_QUICK_SKILL_VALIDATE_CALL) ==
        image + RVA_SKILL_VALIDATE ||
        relative_call_target(image + RVA_QUICK_MENU_VALIDATE_CALL) ==
        image + RVA_SKILL_VALIDATE ||
        relative_call_target(image + RVA_USE_INTERNAL_VALIDATE_CALL) ==
        image + RVA_SKILL_VALIDATE) {
        fputs("FAIL: one or more skill-validator calls were not redirected\n", stderr);
        ++failures;
    }
    if (relative_call_target(image + RVA_QUICK_MENU_SPIRIT_VALIDATE_CALL) ==
        image + RVA_SPIRIT_STRIKE_VALIDATE ||
        relative_call_target(image + RVA_QUICK_MENU_SPIRIT_ACTIVATE_CALL) ==
        image + RVA_SPIRIT_STRIKE_ACTIVATE) {
        fputs("FAIL: one or more Spirit Strike calls were not redirected\n", stderr);
        ++failures;
    }
    if (relative_call_target(image + RVA_USE_CALL) == image + RVA_USE) {
        fputs("FAIL: Use call was not redirected\n", stderr);
        ++failures;
    }
    if (relative_call_target(image + RVA_DIRECT_USE_CALL) == image + RVA_USE) {
        fputs("FAIL: direct Use call was not redirected\n", stderr);
        ++failures;
    }
    if (relative_call_target(image + RVA_STOP_RUMBLE_CALL) ==
        image + RVA_STOP_RUMBLE) {
        fputs("FAIL: StopRumble call was not redirected\n", stderr);
        ++failures;
    }
    if (*(void **)(image + RVA_SCRIPT_CALL_OPCODE_SLOT) ==
        image + RVA_SCRIPT_CALL_OPCODE) {
        fputs("FAIL: script-call opcode slot was not redirected\n", stderr);
        ++failures;
    }
    if (*(void **)(image + RVA_SCRIPT_METHOD_OPCODE_SLOT) ==
        image + RVA_SCRIPT_METHOD_OPCODE) {
        fputs("FAIL: script-method opcode slot was not redirected\n", stderr);
        ++failures;
    }
    if (relative_call_target(image + RVA_SCRIPT_METHOD_BINDING_CALL) ==
        image + RVA_SCRIPT_BINDING_INVOKE) {
        fputs("FAIL: script binding invoke call was not redirected\n", stderr);
        ++failures;
    }
    for (index = 0; index < sizeof(expected_exports) / sizeof(expected_exports[0]);
            ++index) {
        if (*(const uint32_t *)(image + expected_exports[index].slot_rva) ==
            expected_exports[index].function_rva) {
            fprintf(stderr, "FAIL: export %lu was not redirected\n",
                (unsigned long)index);
            ++failures;
        }
    }

    SudekiMpUninstallPlayerInputTrace();
    if (relative_call_target(image + RVA_PLAYER_MOVE_CALL_ALTERNATE) !=
            image + RVA_ARBITER_MOVEMENT ||
        relative_call_target(image + RVA_PLAYER_MOVE_CALL_NORMAL) !=
            image + RVA_ARBITER_MOVEMENT) {
        fputs("FAIL: one or more player movement calls were not restored\n",
            stderr);
        ++failures;
    }
    SudekiMpUninstallSplitScreenRender();
    if (SudekiMpSplitScreenGetRosterTypes(
            &roster_player_one_type, &roster_player_two_type)) {
        fputs("FAIL: split-screen uninstall retained the co-op roster contract\n",
            stderr);
        ++failures;
    }
    if (*(void **)(image + RVA_QUICK_MENU_RENDER_SUBMIT_VTABLE_SLOT) !=
            image + RVA_QUICK_MENU_RENDER_SUBMIT) {
        fputs("FAIL: Quick Menu render-submit vtable slot was not restored\n",
            stderr);
        ++failures;
    }
    if (*(void **)(image + RVA_MOTION_BLUR_POST_RENDER_VTABLE_SLOT) !=
            image + RVA_MOTION_BLUR_POST_RENDER ||
        *(void **)(image + RVA_SCREENSHOT_POST_RENDER_VTABLE_SLOT) !=
            image + RVA_SCREENSHOT_POST_RENDER) {
        fputs("FAIL: Spirit viewport effect callbacks were not restored\n",
            stderr);
        ++failures;
    }
    if (memcmp(
            image + RVA_CAMERA_MANAGER_SET_RENDER_CAMERA,
            set_render_camera_original,
            sizeof(set_render_camera_original)) != 0) {
        fputs("FAIL: SetRenderCamera inline hook was not restored\n", stderr);
        ++failures;
    }
    if (relative_call_target(image + RVA_FRAME_END_CALL_MAIN) !=
            image + RVA_FRAME_END) {
        fputs("FAIL: frame-end compositor call was not restored\n", stderr);
        ++failures;
    }
    if (relative_call_target(image + RVA_RENDER_START_CALL_MAIN) !=
            image + RVA_RENDER_START) {
        fputs("FAIL: render-only camera start call was not restored\n",
            stderr);
        ++failures;
    }
    if (*(void **)(image + RVA_CAMERA_INPUT_EVENT_VTABLE_SLOT) !=
            image + RVA_CAMERA_INPUT_EVENT) {
        fputs("FAIL: Player 2 native camera input gate was not restored\n",
            stderr);
        ++failures;
    }
    if (relative_call_target(image + RVA_PC_QUIT_SCREEN_RENDER_CALL) !=
            image + RVA_PC_QUIT_SCREEN_RENDER) {
        fputs("FAIL: pre-Quit cached-backdrop call was not restored\n",
            stderr);
        ++failures;
    }
    if (relative_call_target(image + RVA_HUD_GROUP_VALUES_POINTER_CALL) !=
            image + RVA_HUD_PARTY_POINTER_COPY ||
        relative_call_target(image + RVA_HUD_GIZMO_PORTRAIT_POINTER_CALL) !=
            image + RVA_HUD_PARTY_POINTER_COPY ||
        relative_call_target(
            image + RVA_HUD_PORTRAIT_RESOURCE_ASSIGNMENT_CALL) !=
            image + RVA_HUD_PORTRAIT_RESOURCE_ASSIGNMENT ||
        relative_call_target(image + RVA_HUD_GIZMO_VALUES_POINTER_CALL) !=
            image + RVA_HUD_PARTY_POINTER_COPY ||
        relative_call_target(image + RVA_HUD_GIZMO_NAME_POINTER_CALL) !=
            image + RVA_HUD_PARTY_POINTER_COPY ||
        relative_call_target(image + RVA_HUD_GIZMO_STATUS_POINTER_CALL) !=
            image + RVA_HUD_PARTY_POINTER_COPY) {
        fputs("FAIL: one or more viewport HUD ownership calls were not restored\n",
            stderr);
        ++failures;
    }
    if (relative_call_target(image + RVA_MINIMAP_UPDATE_POINTER_CALL) !=
            image + RVA_HUD_PARTY_POINTER_COPY ||
        relative_call_target(image + RVA_MINIMAP_RENDER_POINTER_CALL) !=
            image + RVA_HUD_PARTY_POINTER_COPY ||
        memcmp(
            image + RVA_MINIMAP_SNAPSHOT_POINTER_CALL,
            minimap_snapshot_call_original,
            sizeof(minimap_snapshot_call_original)) != 0) {
        fputs("FAIL: one or more viewport minimap ownership calls were not restored\n",
            stderr);
        ++failures;
    }
    if (relative_call_target(image + RVA_RENDER_PHASE_CALL_WORLD_PREPASS) !=
            image + RVA_RENDER_PHASE ||
        relative_call_target(image + RVA_RENDER_PHASE_CALL_WORLD) !=
            image + RVA_RENDER_PHASE ||
        relative_call_target(image + RVA_RENDER_PHASE_CALL_WORLD_OFFSET) !=
            image + RVA_RENDER_PHASE) {
        fputs("FAIL: one or more render-phase calls were not restored\n",
            stderr);
        ++failures;
    }
    SudekiMpUninstallControlSeparation();
    if (*(void **)(image + RVA_CONTROLLER_UPDATE_VTABLE_SLOT) !=
        image + RVA_CONTROLLER_UPDATE) {
        fputs("FAIL: controller update vtable slot was not restored\n", stderr);
        ++failures;
    }
    SudekiMpUninstallTalosDefenseTrace();
    if (memcmp(image + RVA_APPLY_DAMAGE,
            "\x55\x8b\xec\x83\xe4\xf8", 6u) != 0 ||
        memcmp(image + RVA_COLLISION_DAMAGE,
            "\x83\xec\x78\x53\x55", 5u) != 0) {
        fputs("FAIL: Talos defense inline hooks were not restored\n", stderr);
        ++failures;
    }
    SudekiMpUninstallCharacterSwitchTrace();
    if (*(void **)(image + RVA_CHARACTER_INPUT_VTABLE_SLOT) !=
        image + RVA_CHARACTER_INPUT_HANDLER) {
        fputs("FAIL: character input vtable slot was not restored\n", stderr);
        ++failures;
    }
    if (!SudekiMpInstallFreeRoamCameraInput((HMODULE)image, VK_LCONTROL)) {
        fprintf(stderr, "free-roam camera install rejected image (error=%lu)\n",
            (unsigned long)GetLastError());
        ++failures;
    } else {
        if (*(void **)(image + RVA_CHARACTER_INPUT_VTABLE_SLOT) ==
            image + RVA_CHARACTER_INPUT_HANDLER) {
            fputs("FAIL: free-roam camera input slot was not redirected\n",
                stderr);
            ++failures;
        }
        SudekiMpUninstallFreeRoamCameraInput();
        if (*(void **)(image + RVA_CHARACTER_INPUT_VTABLE_SLOT) !=
            image + RVA_CHARACTER_INPUT_HANDLER) {
            fputs("FAIL: free-roam camera input slot was not restored\n",
                stderr);
            ++failures;
        }
    }
    SudekiMpUninstallSpiritStrikeInput();
    if (relative_call_target(image + RVA_MAIN_FRAME_UPDATE_CALL) !=
        image + RVA_MAIN_FRAME_UPDATE) {
        fputs("FAIL: main-frame input poll call was not restored\n", stderr);
        ++failures;
    }
    SudekiMpUninstallQuickSkillInputTrace();
    if (relative_call_target(image + RVA_QUICK_SKILL_ACTION_CALL) !=
        image + RVA_QUICK_SKILL_ACTION ||
        relative_call_target(image + RVA_QUICK_SKILL_VALIDATE_CALL) !=
        image + RVA_SKILL_VALIDATE ||
        relative_call_target(image + RVA_QUICK_MENU_VALIDATE_CALL) !=
        image + RVA_SKILL_VALIDATE ||
        relative_call_target(image + RVA_USE_INTERNAL_VALIDATE_CALL) !=
        image + RVA_SKILL_VALIDATE ||
        relative_call_target(image + RVA_QUICK_MENU_SPIRIT_VALIDATE_CALL) !=
        image + RVA_SPIRIT_STRIKE_VALIDATE ||
        relative_call_target(image + RVA_QUICK_MENU_SPIRIT_ACTIVATE_CALL) !=
        image + RVA_SPIRIT_STRIKE_ACTIVATE) {
        fputs("FAIL: one or more QuickSkill hooks were not restored\n", stderr);
        ++failures;
    }
    SudekiMpUninstallSkillTrace();
    SudekiMpResetSkillActivationAbi();
    if (relative_call_target(image + RVA_USE_CALL) != image + RVA_USE) {
        fputs("FAIL: Use call was not restored\n", stderr);
        ++failures;
    }
    if (relative_call_target(image + RVA_DIRECT_USE_CALL) != image + RVA_USE) {
        fputs("FAIL: direct Use call was not restored\n", stderr);
        ++failures;
    }
    if (relative_call_target(image + RVA_STOP_RUMBLE_CALL) !=
        image + RVA_STOP_RUMBLE) {
        fputs("FAIL: StopRumble call was not restored\n", stderr);
        ++failures;
    }
    if (*(void **)(image + RVA_SCRIPT_CALL_OPCODE_SLOT) !=
        image + RVA_SCRIPT_CALL_OPCODE) {
        fputs("FAIL: script-call opcode slot was not restored\n", stderr);
        ++failures;
    }
    if (*(void **)(image + RVA_SCRIPT_METHOD_OPCODE_SLOT) !=
        image + RVA_SCRIPT_METHOD_OPCODE) {
        fputs("FAIL: script-method opcode slot was not restored\n", stderr);
        ++failures;
    }
    if (relative_call_target(image + RVA_SCRIPT_METHOD_BINDING_CALL) !=
        image + RVA_SCRIPT_BINDING_INVOKE) {
        fputs("FAIL: script binding invoke call was not restored\n", stderr);
        ++failures;
    }
    for (index = 0; index < sizeof(expected_exports) / sizeof(expected_exports[0]);
            ++index) {
        if (*(const uint32_t *)(image + expected_exports[index].slot_rva) !=
            expected_exports[index].function_rva) {
            fprintf(stderr, "FAIL: export %lu was not restored\n",
                (unsigned long)index);
            ++failures;
        }
    }

    VirtualFree(image, 0, MEM_RELEASE);
    if (failures != 0) {
        fprintf(stderr, "%d image hook test(s) failed\n", failures);
        return 1;
    }
    puts("skill_trace_image_test: PASS");
    return 0;
}
