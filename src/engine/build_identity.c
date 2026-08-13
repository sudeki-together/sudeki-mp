#include "engine/build_identity.h"

#include "engine/sha256.h"

#include <string.h>

static const IMAGE_NT_HEADERS32 *get_nt_headers(const unsigned char *base) {
    const IMAGE_DOS_HEADER *dos = (const IMAGE_DOS_HEADER *)base;
    const IMAGE_NT_HEADERS32 *nt;

    if (dos == NULL || dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew <= 0) {
        return NULL;
    }

    nt = (const IMAGE_NT_HEADERS32 *)(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE ||
        nt->FileHeader.Machine != IMAGE_FILE_MACHINE_I386 ||
        nt->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR32_MAGIC) {
        return NULL;
    }

    return nt;
}

BOOL SudekiMpCheckExecutableFile(
    const wchar_t *path,
    SudekiMpBuildCheck *result
) {
    HANDLE file;
    HANDLE mapping;
    const unsigned char *view;
    const IMAGE_NT_HEADERS32 *nt;
    LARGE_INTEGER size;

    if (path == NULL || result == NULL) {
        return FALSE;
    }

    ZeroMemory(result, sizeof(*result));
    if (!SudekiMpSha256File(path, result->actual_sha256)) {
        return FALSE;
    }
    result->hash_matches =
        strcmp(result->actual_sha256, SUDEKIMP_EXPECTED_SHA256) == 0;

    file = CreateFileW(
        path,
        GENERIC_READ,
        FILE_SHARE_READ,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );
    if (file == INVALID_HANDLE_VALUE) {
        return FALSE;
    }

    mapping = NULL;
    view = NULL;
    if (GetFileSizeEx(file, &size) && size.QuadPart >= (LONGLONG)sizeof(IMAGE_DOS_HEADER)) {
        mapping = CreateFileMappingW(file, NULL, PAGE_READONLY, 0, 0, NULL);
    }
    if (mapping != NULL) {
        view = (const unsigned char *)MapViewOfFile(mapping, FILE_MAP_READ, 0, 0, 0);
    }

    nt = get_nt_headers(view);
    if (nt != NULL) {
        result->pe_matches =
            nt->FileHeader.TimeDateStamp == SUDEKIMP_EXPECTED_TIMESTAMP &&
            nt->OptionalHeader.SizeOfImage == SUDEKIMP_EXPECTED_IMAGE_SIZE;
    }

    if (view != NULL) {
        UnmapViewOfFile(view);
    }
    if (mapping != NULL) {
        CloseHandle(mapping);
    }
    CloseHandle(file);
    return TRUE;
}

BOOL SudekiMpCheckLoadedExecutable(HMODULE module) {
    const IMAGE_NT_HEADERS32 *nt = get_nt_headers((const unsigned char *)module);
    if (nt == NULL) {
        return FALSE;
    }

    return nt->FileHeader.TimeDateStamp == SUDEKIMP_EXPECTED_TIMESTAMP &&
        nt->OptionalHeader.SizeOfImage == SUDEKIMP_EXPECTED_IMAGE_SIZE;
}
