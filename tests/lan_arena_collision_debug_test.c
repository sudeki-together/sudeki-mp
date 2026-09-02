#include "hooks/lan_arena_collision_debug.h"

#include <windows.h>

#include <stdint.h>
#include <stdio.h>
#include <string.h>

enum {
    RVA_TOGGLE_COLLISION_SPHERES = 0x000e6a10u,
    RVA_TOGGLE_COLLISION_MESH = 0x000e6a20u,
    RVA_TOGGLE_SPHERE_TREES = 0x000e6a30u,
    RVA_COLLISION_SPHERES_FLAG = 0x003c2fdbu,
    RVA_COLLISION_MESH_FLAG = 0x003c2fe0u,
    RVA_SPHERE_TREES_FLAG = 0x003c2fe1u,
    TOGGLE_SIZE = 16u,
    TOGGLE_COUNT = 3u
};

static int failures;

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "FAIL %s:%d: %s\n", \
            __FILE__, __LINE__, #condition); \
        ++failures; \
    } \
} while (0)

static uint8_t *read_file(const wchar_t *path, DWORD *file_size) {
    HANDLE file;
    uint8_t *data;
    DWORD bytes_read;

    file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) return NULL;
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

    if (file == NULL || file_size < sizeof(IMAGE_DOS_HEADER)) return NULL;
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
    if (image == NULL) return NULL;
    memcpy(image, file, nt->OptionalHeader.SizeOfHeaders);
    sections = IMAGE_FIRST_SECTION(nt);
    for (index = 0u; index < nt->FileHeader.NumberOfSections; ++index) {
        DWORD source = sections[index].PointerToRawData;
        DWORD size = sections[index].SizeOfRawData;
        DWORD destination = sections[index].VirtualAddress;
        if (source > file_size || size > file_size - source ||
            destination > nt->OptionalHeader.SizeOfImage ||
            size > nt->OptionalHeader.SizeOfImage - destination) {
            VirtualFree(image, 0, MEM_RELEASE);
            return NULL;
        }
        if (size != 0u) memcpy(image + destination, file + source, size);
    }
    return image;
}

static void relocate_toggle_operands(uint8_t *image) {
    const uintptr_t entry_rvas[TOGGLE_COUNT] = {
        RVA_TOGGLE_COLLISION_SPHERES,
        RVA_TOGGLE_COLLISION_MESH,
        RVA_TOGGLE_SPHERE_TREES
    };
    const uintptr_t flag_rvas[TOGGLE_COUNT] = {
        RVA_COLLISION_SPHERES_FLAG,
        RVA_COLLISION_MESH_FLAG,
        RVA_SPHERE_TREES_FLAG
    };
    unsigned int index;

    for (index = 0u; index < TOGGLE_COUNT; ++index) {
        uint32_t relocated_flag =
            (uint32_t)(uintptr_t)(image + flag_rvas[index]);
        memcpy(image + entry_rvas[index] + 2u,
            &relocated_flag, sizeof(relocated_flag));
        memcpy(image + entry_rvas[index] + 11u,
            &relocated_flag, sizeof(relocated_flag));
    }
}

static void test_edge_policy(void) {
    BOOL was_down = FALSE;

    CHECK(!SudekiMpLanArenaCollisionDebugConsumeEdgeForTesting(
        FALSE, TRUE, &was_down));
    CHECK(was_down);
    CHECK(!SudekiMpLanArenaCollisionDebugConsumeEdgeForTesting(
        TRUE, TRUE, &was_down));
    CHECK(!SudekiMpLanArenaCollisionDebugConsumeEdgeForTesting(
        TRUE, FALSE, &was_down));
    CHECK(!was_down);
    CHECK(SudekiMpLanArenaCollisionDebugConsumeEdgeForTesting(
        TRUE, TRUE, &was_down));
    CHECK(!SudekiMpLanArenaCollisionDebugConsumeEdgeForTesting(
        TRUE, TRUE, &was_down));
    CHECK(!SudekiMpLanArenaCollisionDebugConsumeEdgeForTesting(
        TRUE, FALSE, NULL));
}

static void test_exact_image(uint8_t *image) {
    const uintptr_t entry_rvas[TOGGLE_COUNT] = {
        RVA_TOGGLE_COLLISION_SPHERES,
        RVA_TOGGLE_COLLISION_MESH,
        RVA_TOGGLE_SPHERE_TREES
    };
    uint8_t saved_toggles[TOGGLE_COUNT][TOGGLE_SIZE];
    uint8_t *sphere_flag = image + RVA_COLLISION_SPHERES_FLAG;
    uint8_t *mesh_flag = image + RVA_COLLISION_MESH_FLAG;
    uint8_t *tree_flag = image + RVA_SPHERE_TREES_FLAG;
    unsigned int index;

    relocate_toggle_operands(image);
    for (index = 0u; index < TOGGLE_COUNT; ++index) {
        memcpy(saved_toggles[index], image + entry_rvas[index], TOGGLE_SIZE);
    }

    image[RVA_TOGGLE_COLLISION_MESH] ^= 0x01u;
    SetLastError(ERROR_SUCCESS);
    CHECK(!SudekiMpInstallLanArenaCollisionDebug((HMODULE)image));
    CHECK(GetLastError() == ERROR_BAD_EXE_FORMAT);
    CHECK(SudekiMpLanArenaCollisionDebugMode() ==
        SUDEKIMP_LAN_ARENA_COLLISION_DEBUG_OFF);
    image[RVA_TOGGLE_COLLISION_MESH] ^= 0x01u;

    image[RVA_TOGGLE_SPHERE_TREES + 2u] ^= 0x01u;
    SetLastError(ERROR_SUCCESS);
    CHECK(!SudekiMpInstallLanArenaCollisionDebug((HMODULE)image));
    CHECK(GetLastError() == ERROR_BAD_EXE_FORMAT);
    image[RVA_TOGGLE_SPHERE_TREES + 2u] ^= 0x01u;

    *sphere_flag = 0u;
    *mesh_flag = 2u;
    *tree_flag = 0u;
    SetLastError(ERROR_SUCCESS);
    CHECK(!SudekiMpInstallLanArenaCollisionDebug((HMODULE)image));
    CHECK(GetLastError() == ERROR_INVALID_DATA);
    CHECK(*sphere_flag == 0u && *mesh_flag == 2u && *tree_flag == 0u);

    *sphere_flag = 0u;
    *mesh_flag = 1u;
    *tree_flag = 1u;
    CHECK(SudekiMpInstallLanArenaCollisionDebug((HMODULE)image));
    CHECK(*sphere_flag == 0u && *mesh_flag == 0u && *tree_flag == 0u);
    CHECK(SudekiMpLanArenaCollisionDebugMode() ==
        SUDEKIMP_LAN_ARENA_COLLISION_DEBUG_OFF);

    SetLastError(ERROR_SUCCESS);
    CHECK(!SudekiMpInstallLanArenaCollisionDebug((HMODULE)image));
    CHECK(GetLastError() == ERROR_ALREADY_EXISTS);
    CHECK(*sphere_flag == 0u && *mesh_flag == 0u && *tree_flag == 0u);

    CHECK(SudekiMpLanArenaCollisionDebugAdvanceForTesting());
    CHECK(SudekiMpLanArenaCollisionDebugMode() ==
        SUDEKIMP_LAN_ARENA_COLLISION_DEBUG_SPHERES);
    CHECK(*sphere_flag == 1u && *mesh_flag == 0u && *tree_flag == 0u);

    CHECK(SudekiMpLanArenaCollisionDebugAdvanceForTesting());
    CHECK(SudekiMpLanArenaCollisionDebugMode() ==
        SUDEKIMP_LAN_ARENA_COLLISION_DEBUG_SPHERES_AND_MESH);
    CHECK(*sphere_flag == 1u && *mesh_flag == 1u && *tree_flag == 0u);

    CHECK(SudekiMpLanArenaCollisionDebugAdvanceForTesting());
    CHECK(SudekiMpLanArenaCollisionDebugMode() ==
        SUDEKIMP_LAN_ARENA_COLLISION_DEBUG_OFF);
    CHECK(*sphere_flag == 0u && *mesh_flag == 0u && *tree_flag == 0u);

    SudekiMpUninstallLanArenaCollisionDebug();
    CHECK(SudekiMpLanArenaCollisionDebugMode() ==
        SUDEKIMP_LAN_ARENA_COLLISION_DEBUG_OFF);
    CHECK(*sphere_flag == 0u && *mesh_flag == 1u && *tree_flag == 1u);
    for (index = 0u; index < TOGGLE_COUNT; ++index) {
        CHECK(memcmp(image + entry_rvas[index], saved_toggles[index],
            TOGGLE_SIZE) == 0);
    }
    CHECK(!SudekiMpLanArenaCollisionDebugAdvanceForTesting());
}

int wmain(int argc, wchar_t **argv) {
    uint8_t *file;
    uint8_t *image;
    DWORD file_size;

    test_edge_policy();
    if (argc != 2) {
        fwprintf(stderr,
            L"usage: SudekiMP.LanArenaCollisionDebugTest.exe SUDEKI.exe\n");
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
    test_exact_image(image);
    SudekiMpUninstallLanArenaCollisionDebug();
    VirtualFree(image, 0, MEM_RELEASE);
    if (failures != 0) {
        fprintf(stderr, "%d LAN collision debug test(s) failed\n", failures);
        return 1;
    }
    puts("lan_arena_collision_debug_test: PASS");
    return 0;
}
