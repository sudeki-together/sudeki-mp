#include "engine/skill_activation_abi.h"
#include "hooks/skill_trace.h"
#include "hooks/character_switch_trace.h"
#include "hooks/control_separation.h"
#include "hooks/freeroam_camera_input.h"
#include "hooks/player_input_trace.h"
#include "hooks/quick_skill_input.h"
#include "hooks/spirit_strike_input.h"
#include "hooks/split_screen_render.h"

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
    RVA_CONTROLLER_UPDATE = 0x00027cf0u,
    RVA_CONTROLLER_UPDATE_VTABLE_SLOT = 0x002c9f60u,
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
    RVA_SKILL_VALIDATE_FLAG = 0x0034a8b0u
};

typedef struct ExpectedExport {
    uint32_t slot_rva;
    uint32_t function_rva;
} ExpectedExport;

typedef struct ExpectedEntry {
    uint32_t function_rva;
    uint8_t bytes[8];
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

int wmain(int argc, wchar_t **argv) {
    uint8_t *file;
    uint8_t *image;
    DWORD file_size;
    size_t index;
    int failures = 0;
    unsigned int quick_menu_isolation_state;
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

    /* Simulate the loader relocation for the absolute jump-table pointer. */
    *(void **)(image + RVA_SCRIPT_CALL_OPCODE_SLOT) =
        image + RVA_SCRIPT_CALL_OPCODE;
    *(void **)(image + RVA_SCRIPT_METHOD_OPCODE_SLOT) =
        image + RVA_SCRIPT_METHOD_OPCODE;
    *(void **)(image + RVA_CHARACTER_INPUT_VTABLE_SLOT) =
        image + RVA_CHARACTER_INPUT_HANDLER;
    *(void **)(image + RVA_CONTROLLER_UPDATE_VTABLE_SLOT) =
        image + RVA_CONTROLLER_UPDATE;
    *(void **)(image + RVA_MOTION_BLUR_POST_RENDER_VTABLE_SLOT) =
        image + RVA_MOTION_BLUR_POST_RENDER;
    *(void **)(image + RVA_SCREENSHOT_POST_RENDER_VTABLE_SLOT) =
        image + RVA_SCREENSHOT_POST_RENDER;
    *(void **)(image + RVA_QUICK_MENU_RENDER_SUBMIT_VTABLE_SLOT) =
        image + RVA_QUICK_MENU_RENDER_SUBMIT;
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
    *(uint32_t *)(image + RVA_CHARACTER_TYPE_TO_PORTRAIT_ENUM + 9u) =
        (uint32_t)(uintptr_t)(image + RVA_CHARACTER_TYPE_TO_PORTRAIT_LOOKUP);
    *(uint32_t *)(image + RVA_HUD_PORTRAIT_RESOURCE_SELECT + 8u) =
        (uint32_t)(uintptr_t)(image + RVA_UI_RESOURCE_TABLE_INITIALIZED);
    *(uint32_t *)(image + RVA_SKILL_DATA_AVAILABLE + 2u) =
        (uint32_t)(uintptr_t)(image + RVA_SKILL_AVAILABILITY_FLAG);
    *(uint32_t *)(image + RVA_SKILL_VALIDATE + 2u) =
        (uint32_t)(uintptr_t)(image + RVA_SKILL_VALIDATE_FLAG);

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
    if (!SudekiMpInstallCharacterSwitchTrace((HMODULE)image)) {
        fprintf(stderr, "character-switch trace install rejected image (error=%lu)\n",
            (unsigned long)GetLastError());
        SudekiMpUninstallSpiritStrikeInput();
        SudekiMpUninstallQuickSkillInputTrace();
        SudekiMpUninstallSkillTrace();
        VirtualFree(image, 0, MEM_RELEASE);
        return 1;
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
    *(void **)(image + RVA_QUICK_MENU_RENDER_SUBMIT_VTABLE_SLOT) =
        image + RVA_QUICK_MENU_IS_ACTIVE;
    if (SudekiMpInstallSplitScreenRender(
            (HMODULE)image,
            TRUE,
            TRUE,
            VK_F9,
            TRUE,
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
    if (!SudekiMpInstallSplitScreenRender(
            (HMODULE)image,
            TRUE,
            TRUE,
            VK_F9,
            TRUE,
            FALSE,
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
