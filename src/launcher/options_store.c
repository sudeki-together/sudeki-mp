#include "options_store.h"

#include <shlobj.h>
#include <strsafe.h>

#include <stdlib.h>
#include <string.h>

/* Sudeki's option document is ~30 KiB; refuse anything implausibly large. */
#define SUDEKIMP_OPTIONS_MAX_BYTES (1024u * 1024u)

static BOOL options_directory(WCHAR *path, size_t path_count) {
    WCHAR app_data[MAX_PATH];
    if (FAILED(SHGetFolderPathW(NULL, CSIDL_APPDATA, NULL, SHGFP_TYPE_CURRENT,
                                app_data))) {
        return FALSE;
    }
    return SUCCEEDED(StringCchPrintfW(path, path_count, L"%s\\Sudeki", app_data));
}

BOOL SudekiMpOptionsStorePlayerPath(WCHAR *path, size_t path_count) {
    WCHAR directory[MAX_PATH];
    return options_directory(directory, sizeof(directory) / sizeof(directory[0])) &&
           SUCCEEDED(StringCchPrintfW(path, path_count, L"%s\\PlayerOptions.xml",
                                      directory));
}

static BOOL read_file_bytes(const WCHAR *path,
                            unsigned char **bytes,
                            DWORD *count,
                            DWORD *error) {
    HANDLE file;
    LARGE_INTEGER size;
    DWORD read = 0u;
    *bytes = NULL;
    *count = 0u;
    *error = ERROR_SUCCESS;
    file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
                       FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        *error = GetLastError();
        return FALSE;
    }
    if (!GetFileSizeEx(file, &size) || size.QuadPart <= 0 ||
        size.QuadPart > SUDEKIMP_OPTIONS_MAX_BYTES) {
        *error = ERROR_INVALID_DATA;
        CloseHandle(file);
        return FALSE;
    }
    *bytes = (unsigned char *)malloc((size_t)size.QuadPart);
    if (*bytes == NULL ||
        !ReadFile(file, *bytes, (DWORD)size.QuadPart, &read, NULL) ||
        read != (DWORD)size.QuadPart) {
        *error = ERROR_READ_FAULT;
        free(*bytes);
        *bytes = NULL;
        CloseHandle(file);
        return FALSE;
    }
    CloseHandle(file);
    *count = read;
    return TRUE;
}

static BOOL load_file(const WCHAR *path, SudekiMpPlayerOptions *options, DWORD *error) {
    unsigned char *bytes;
    DWORD count;
    BOOL loaded;
    if (!read_file_bytes(path, &bytes, &count, error)) {
        return FALSE;
    }
    loaded = SudekiMpPlayerOptionsLoad(options, bytes, count) != 0;
    free(bytes);
    if (!loaded) {
        *error = ERROR_INVALID_DATA;
    }
    return loaded;
}

BOOL SudekiMpOptionsStoreLoadDefaults(const WCHAR *game_directory,
                                      SudekiMpPlayerOptions *options) {
    WCHAR path[MAX_PATH];
    DWORD error;
    if (game_directory == NULL || game_directory[0] == L'\0' ||
        FAILED(StringCchPrintfW(path, MAX_PATH,
                                L"%s\\launcherdata\\DefaultOptions.xml",
                                game_directory)) ||
        !load_file(path, options, &error)) {
        return FALSE;
    }
    /* SudekiLauncher.exe writes PlayerOptions.xml as UTF-16LE with a BOM. */
    options->encoding = SUDEKIMP_PLAYER_OPTIONS_UTF16LE;
    options->has_bom = 1;
    return TRUE;
}

SudekiMpOptionsSource SudekiMpOptionsStoreLoad(const WCHAR *game_directory,
                                               SudekiMpPlayerOptions *options) {
    WCHAR path[MAX_PATH];
    DWORD error = ERROR_SUCCESS;
    if (!SudekiMpOptionsStorePlayerPath(path, sizeof(path) / sizeof(path[0]))) {
        return SUDEKIMP_OPTIONS_SOURCE_NONE;
    }
    if (load_file(path, options, &error)) {
        return SUDEKIMP_OPTIONS_SOURCE_PLAYER;
    }
    if ((error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND) &&
        SudekiMpOptionsStoreLoadDefaults(game_directory, options)) {
        return SUDEKIMP_OPTIONS_SOURCE_DEFAULTS;
    }
    return SUDEKIMP_OPTIONS_SOURCE_NONE;
}

static BOOL write_new_file(const WCHAR *path, const unsigned char *bytes, size_t count) {
    HANDLE file = CreateFileW(path, GENERIC_WRITE, 0u, NULL, CREATE_ALWAYS,
                              FILE_ATTRIBUTE_NORMAL, NULL);
    DWORD written = 0u;
    BOOL ok;
    if (file == INVALID_HANDLE_VALUE) {
        return FALSE;
    }
    ok = WriteFile(file, bytes, (DWORD)count, &written, NULL) && written == count &&
         FlushFileBuffers(file);
    CloseHandle(file);
    if (!ok) {
        DeleteFileW(path);
    }
    return ok;
}

BOOL SudekiMpOptionsStoreSave(const SudekiMpPlayerOptions *options) {
    WCHAR directory[MAX_PATH];
    WCHAR path[MAX_PATH];
    WCHAR temporary[MAX_PATH];
    WCHAR backup[MAX_PATH];
    unsigned char *bytes = NULL;
    unsigned char *verify = NULL;
    size_t count = 0u;
    DWORD verify_count = 0u;
    DWORD error;
    BOOL ok = FALSE;

    if (!options_directory(directory, sizeof(directory) / sizeof(directory[0])) ||
        !SudekiMpOptionsStorePlayerPath(path, sizeof(path) / sizeof(path[0])) ||
        FAILED(StringCchPrintfW(temporary, MAX_PATH, L"%s.sudekimp-tmp", path)) ||
        FAILED(StringCchPrintfW(backup, MAX_PATH, L"%s.sudekimp-backup", path)) ||
        !SudekiMpPlayerOptionsSerialize(options, &bytes, &count) ||
        count == 0u || count > SUDEKIMP_OPTIONS_MAX_BYTES) {
        free(bytes);
        return FALSE;
    }
    if (GetFileAttributesW(directory) == INVALID_FILE_ATTRIBUTES &&
        !CreateDirectoryW(directory, NULL)) {
        free(bytes);
        return FALSE;
    }
    if (write_new_file(temporary, bytes, count)) {
        /* Keep the first original the launcher ever replaced; later saves
           must not overwrite it with launcher-written content. */
        if (GetFileAttributesW(path) != INVALID_FILE_ATTRIBUTES) {
            CopyFileW(path, backup, TRUE);
        }
        if (MoveFileExW(temporary, path,
                        MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
            ok = read_file_bytes(path, &verify, &verify_count, &error) &&
                 verify_count == count && memcmp(verify, bytes, count) == 0;
        } else {
            DeleteFileW(temporary);
        }
    }
    free(verify);
    free(bytes);
    return ok;
}
