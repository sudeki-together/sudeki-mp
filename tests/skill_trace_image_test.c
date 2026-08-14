#include "hooks/skill_trace.h"
#include "hooks/character_switch_trace.h"
#include "hooks/control_separation.h"
#include "hooks/freeroam_camera_input.h"
#include "hooks/player_input_trace.h"
#include "hooks/quick_skill_input.h"
#include "hooks/spirit_strike_input.h"

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
    RVA_STOP_RUMBLE = 0x000b50d0u,
    RVA_STOP_RUMBLE_CALL = 0x000b4f23u,
    RVA_SCRIPT_CALL_OPCODE = 0x001c4970u,
    RVA_SCRIPT_CALL_OPCODE_SLOT = 0x00323fa0u,
    RVA_SCRIPT_METHOD_OPCODE = 0x001c4b10u,
    RVA_SCRIPT_METHOD_OPCODE_SLOT = 0x00323fa4u,
    RVA_SCRIPT_METHOD_BINDING_CALL = 0x001c4c2fu,
    RVA_SCRIPT_BINDING_INVOKE = 0x002351c0u
};

typedef struct ExpectedExport {
    uint32_t slot_rva;
    uint32_t function_rva;
} ExpectedExport;

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

    /* Simulate the loader relocation for the absolute jump-table pointer. */
    *(void **)(image + RVA_SCRIPT_CALL_OPCODE_SLOT) =
        image + RVA_SCRIPT_CALL_OPCODE;
    *(void **)(image + RVA_SCRIPT_METHOD_OPCODE_SLOT) =
        image + RVA_SCRIPT_METHOD_OPCODE;
    *(void **)(image + RVA_CHARACTER_INPUT_VTABLE_SLOT) =
        image + RVA_CHARACTER_INPUT_HANDLER;
    *(void **)(image + RVA_CONTROLLER_UPDATE_VTABLE_SLOT) =
        image + RVA_CONTROLLER_UPDATE;

    if (!SudekiMpInstallSkillTrace((HMODULE)image, 1.0f, 1.0f)) {
        fprintf(stderr, "install rejected image (error=%lu)\n",
            (unsigned long)GetLastError());
        VirtualFree(image, 0, MEM_RELEASE);
        return 1;
    }
    if (!SudekiMpInstallQuickSkillInputTrace((HMODULE)image, TRUE)) {
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
    if (!SudekiMpInstallControlSeparation((HMODULE)image, 'J', TRUE)) {
        fprintf(stderr, "control-separation install rejected image (error=%lu)\n",
            (unsigned long)GetLastError());
        SudekiMpUninstallCharacterSwitchTrace();
        SudekiMpUninstallSpiritStrikeInput();
        SudekiMpUninstallQuickSkillInputTrace();
        SudekiMpUninstallSkillTrace();
        VirtualFree(image, 0, MEM_RELEASE);
        return 1;
    }
    if (!SudekiMpInstallPlayerInputTrace((HMODULE)image)) {
        fprintf(stderr, "player-input trace install rejected image (error=%lu)\n",
            (unsigned long)GetLastError());
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
