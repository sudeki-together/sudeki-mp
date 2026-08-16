#include "engine/build_identity.h"

#include <windows.h>
#include <tlhelp32.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>

#define SUDEKIMP_MAX_COMMAND_LINE 32768u

typedef DWORD (WINAPI *RemoteThreadRoutine)(void *);

typedef struct RemoteUnicodeString32 {
    USHORT length;
    USHORT maximum_length;
    uint32_t buffer;
} RemoteUnicodeString32;

typedef struct RemoteLdrData {
    RemoteUnicodeString32 name;
    uint32_t module;
    wchar_t path[MAX_PATH];
} RemoteLdrData;

static void print_error(const wchar_t *operation) {
    fwprintf(stderr, L"SudekiMP launcher: %ls failed (Win32 error %lu)\n",
        operation, (unsigned long)GetLastError());
}

static BOOL absolute_path(const wchar_t *input, wchar_t output[MAX_PATH]) {
    DWORD length = GetFullPathNameW(input, MAX_PATH, output, NULL);
    return length != 0 && length < MAX_PATH;
}

static void directory_from_path(const wchar_t *path, wchar_t directory[MAX_PATH]) {
    wchar_t *slash;
    lstrcpyW(directory, path);
    slash = wcsrchr(directory, L'\\');
    if (slash != NULL) {
        *slash = L'\0';
    } else {
        lstrcpyW(directory, L".");
    }
}

static const wchar_t *basename_from_path(const wchar_t *path) {
    const wchar_t *slash = wcsrchr(path, L'\\');
    return slash == NULL ? path : slash + 1;
}

static HMODULE remote_module_base(DWORD process_id, const wchar_t *module_name) {
    HANDLE snapshot;
    MODULEENTRY32W entry;
    HMODULE result = NULL;

    snapshot = CreateToolhelp32Snapshot(
        TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32,
        process_id
    );
    if (snapshot == INVALID_HANDLE_VALUE) {
        return NULL;
    }

    ZeroMemory(&entry, sizeof(entry));
    entry.dwSize = sizeof(entry);
    if (Module32FirstW(snapshot, &entry)) {
        do {
            if (lstrcmpiW(entry.szModule, module_name) == 0) {
                result = entry.hModule;
                break;
            }
        } while (Module32NextW(snapshot, &entry));
    }

    CloseHandle(snapshot);
    return result;
}

static BOOL executable_protection(DWORD protection) {
    protection &= 0xffu;
    return protection == PAGE_EXECUTE ||
        protection == PAGE_EXECUTE_READ ||
        protection == PAGE_EXECUTE_READWRITE ||
        protection == PAGE_EXECUTE_WRITECOPY;
}

static RemoteThreadRoutine validated_shared_export(
    HANDLE process,
    const wchar_t *module_name,
    const char *export_name
) {
    union {
        FARPROC function;
        const void *address;
    } local_export;
    HMODULE local_module = GetModuleHandleW(module_name);
    MEMORY_BASIC_INFORMATION remote_memory;
    unsigned char local_bytes[16];
    unsigned char remote_bytes[16];
    SIZE_T bytes_read;

    if (local_module == NULL) {
        return NULL;
    }
    local_export.function = GetProcAddress(local_module, export_name);
    if (local_export.function == NULL ||
        VirtualQueryEx(
            process,
            local_export.address,
            &remote_memory,
            sizeof(remote_memory)) == 0 ||
        remote_memory.State != MEM_COMMIT ||
        !executable_protection(remote_memory.Protect)) {
        return NULL;
    }

    CopyMemory(local_bytes, local_export.address, sizeof(local_bytes));
    if (!ReadProcessMemory(
            process,
            local_export.address,
            remote_bytes,
            sizeof(remote_bytes),
            &bytes_read) ||
        bytes_read != sizeof(remote_bytes) ||
        memcmp(local_bytes, remote_bytes, sizeof(local_bytes)) != 0) {
        return NULL;
    }
    return (RemoteThreadRoutine)local_export.function;
}

static RemoteThreadRoutine remote_load_library_address(
    HANDLE process,
    DWORD process_id
) {
    union {
        FARPROC function;
        const void *address;
    } local_load_library;
    MEMORY_BASIC_INFORMATION memory;
    HMODULE local_owner;
    HMODULE remote_owner;
    wchar_t owner_path[MAX_PATH];
    uintptr_t offset;

    local_load_library.function =
        GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "LoadLibraryW");
    if (local_load_library.function == NULL ||
        VirtualQuery(local_load_library.address, &memory, sizeof(memory)) == 0) {
        return NULL;
    }
    local_owner = (HMODULE)memory.AllocationBase;
    if (GetModuleFileNameW(local_owner, owner_path, MAX_PATH) == 0) {
        return NULL;
    }
    remote_owner = remote_module_base(process_id, basename_from_path(owner_path));
    if (remote_owner != NULL) {
        offset = (uintptr_t)local_load_library.address - (uintptr_t)local_owner;
        return (RemoteThreadRoutine)((uintptr_t)remote_owner + offset);
    }

    return validated_shared_export(process, L"kernel32.dll", "LoadLibraryW");
}

static BOOL run_remote_thread(
    HANDLE process,
    RemoteThreadRoutine routine,
    void *parameter,
    DWORD *exit_code
) {
    HANDLE thread = CreateRemoteThread(
        process,
        NULL,
        0,
        (LPTHREAD_START_ROUTINE)routine,
        parameter,
        0,
        NULL
    );
    if (thread == NULL) {
        return FALSE;
    }
    if (WaitForSingleObject(thread, 30000) != WAIT_OBJECT_0 ||
        !GetExitCodeThread(thread, exit_code)) {
        CloseHandle(thread);
        return FALSE;
    }
    CloseHandle(thread);
    return TRUE;
}

static BOOL load_dll_through_ntdll(
    HANDLE process,
    const wchar_t *dll_path,
    DWORD *remote_module_value
) {
    /* push ebp; mov ebp,esp; push module; push unicode; push 0; push 0;
       mov eax,LdrLoadDll; call eax; mov esp,ebp; pop ebp; ret 4 */
    unsigned char code[] = {
        0x55, 0x89, 0xe5,
        0x68, 0, 0, 0, 0,
        0x68, 0, 0, 0, 0,
        0x6a, 0x00,
        0x6a, 0x00,
        0xb8, 0, 0, 0, 0,
        0xff, 0xd0,
        0x89, 0xec,
        0x5d,
        0xc2, 0x04, 0x00
    };
    RemoteThreadRoutine ldr_load_dll =
        validated_shared_export(process, L"ntdll.dll", "LdrLoadDll");
    RemoteLdrData data;
    void *remote_data = NULL;
    void *remote_code = NULL;
    uintptr_t value;
    DWORD old_protection;
    DWORD status;
    SIZE_T bytes_read;
    union {
        void *address;
        RemoteThreadRoutine routine;
    } remote_entry;
    BOOL success = FALSE;
    size_t path_characters = (size_t)lstrlenW(dll_path);

    if (ldr_load_dll == NULL || path_characters >= MAX_PATH) {
        return FALSE;
    }

    ZeroMemory(&data, sizeof(data));
    data.name.length = (USHORT)(path_characters * sizeof(wchar_t));
    data.name.maximum_length = (USHORT)((path_characters + 1u) * sizeof(wchar_t));
    lstrcpyW(data.path, dll_path);

    remote_data = VirtualAllocEx(
        process,
        NULL,
        sizeof(data),
        MEM_COMMIT | MEM_RESERVE,
        PAGE_READWRITE
    );
    remote_code = VirtualAllocEx(
        process,
        NULL,
        sizeof(code),
        MEM_COMMIT | MEM_RESERVE,
        PAGE_READWRITE
    );
    if (remote_data == NULL || remote_code == NULL) {
        goto cleanup;
    }

    data.name.buffer = (uint32_t)(uintptr_t)(
        (unsigned char *)remote_data + offsetof(RemoteLdrData, path)
    );
    if (!WriteProcessMemory(process, remote_data, &data, sizeof(data), NULL)) {
        goto cleanup;
    }

    value = (uintptr_t)((unsigned char *)remote_data + offsetof(RemoteLdrData, module));
    memcpy(&code[4], &value, sizeof(uint32_t));
    value = (uintptr_t)remote_data;
    memcpy(&code[9], &value, sizeof(uint32_t));
    value = (uintptr_t)ldr_load_dll;
    memcpy(&code[18], &value, sizeof(uint32_t));

    if (!WriteProcessMemory(process, remote_code, code, sizeof(code), NULL) ||
        !VirtualProtectEx(
            process,
            remote_code,
            sizeof(code),
            PAGE_EXECUTE_READ,
            &old_protection)) {
        goto cleanup;
    }
    FlushInstructionCache(process, remote_code, sizeof(code));
    remote_entry.address = remote_code;

    if (!run_remote_thread(
            process,
            remote_entry.routine,
            NULL,
            &status) ||
        status != 0) {
        SetLastError(ERROR_DLL_INIT_FAILED);
        goto cleanup;
    }
    if (!ReadProcessMemory(
            process,
            (unsigned char *)remote_data + offsetof(RemoteLdrData, module),
            remote_module_value,
            sizeof(*remote_module_value),
            &bytes_read) ||
        bytes_read != sizeof(*remote_module_value) ||
        *remote_module_value == 0) {
        goto cleanup;
    }
    success = TRUE;

cleanup:
    if (remote_code != NULL) {
        VirtualFreeEx(process, remote_code, 0, MEM_RELEASE);
    }
    if (remote_data != NULL) {
        VirtualFreeEx(process, remote_data, 0, MEM_RELEASE);
    }
    return success;
}

static BOOL inject_and_initialize(
    const PROCESS_INFORMATION *process,
    const wchar_t *dll_path
) {
    SIZE_T path_bytes = ((SIZE_T)lstrlenW(dll_path) + 1u) * sizeof(wchar_t);
    void *remote_path;
    RemoteThreadRoutine load_library;
    DWORD remote_module_value;
    HMODULE local_dll;
    FARPROC local_initialize;
    uintptr_t initialize_rva;
    RemoteThreadRoutine remote_initialize;
    DWORD initialize_result;
    BOOL success = FALSE;

    remote_path = VirtualAllocEx(
        process->hProcess,
        NULL,
        path_bytes,
        MEM_COMMIT | MEM_RESERVE,
        PAGE_READWRITE
    );
    if (remote_path == NULL) {
        print_error(L"VirtualAllocEx");
        return FALSE;
    }
    if (!WriteProcessMemory(
            process->hProcess,
            remote_path,
            dll_path,
            path_bytes,
            NULL)) {
        print_error(L"WriteProcessMemory");
        goto cleanup_remote_path;
    }

    load_library = remote_load_library_address(
        process->hProcess,
        process->dwProcessId
    );
    if (load_library != NULL) {
        if (!run_remote_thread(
                process->hProcess,
                load_library,
                remote_path,
                &remote_module_value) ||
            remote_module_value == 0) {
            print_error(L"remote LoadLibraryW");
            goto cleanup_remote_path;
        }
    } else {
        wprintf(L"Using validated ntdll bootstrap for suspended Wine process.\n");
        if (!load_dll_through_ntdll(
                process->hProcess,
                dll_path,
                &remote_module_value)) {
            print_error(L"remote LdrLoadDll bootstrap");
            goto cleanup_remote_path;
        }
    }

    local_dll = LoadLibraryExW(dll_path, NULL, DONT_RESOLVE_DLL_REFERENCES);
    if (local_dll == NULL) {
        print_error(L"LoadLibraryExW for export lookup");
        goto cleanup_remote_path;
    }
    local_initialize = GetProcAddress(local_dll, "SudekiMP_Initialize");
    if (local_initialize == NULL) {
        print_error(L"GetProcAddress(SudekiMP_Initialize)");
        FreeLibrary(local_dll);
        goto cleanup_remote_path;
    }

    initialize_rva = (uintptr_t)local_initialize - (uintptr_t)local_dll;
    remote_initialize = (RemoteThreadRoutine)(
        (uintptr_t)remote_module_value + initialize_rva
    );
    FreeLibrary(local_dll);

    if (!run_remote_thread(
            process->hProcess,
            remote_initialize,
            NULL,
            &initialize_result)) {
        print_error(L"SudekiMP_Initialize");
        goto cleanup_remote_path;
    }
    if (initialize_result != 0) {
        fwprintf(stderr, L"SudekiMP initialization rejected the process (status %lu).\n",
            (unsigned long)initialize_result);
        goto cleanup_remote_path;
    }

    success = TRUE;

cleanup_remote_path:
    VirtualFreeEx(process->hProcess, remote_path, 0, MEM_RELEASE);
    return success;
}

static BOOL append_command_line_argument(
    wchar_t *command_line,
    size_t capacity,
    const wchar_t *argument
) {
    size_t used;
    size_t argument_length;

    if (command_line == NULL || capacity == 0u || argument == NULL) {
        return FALSE;
    }
    used = wcslen(command_line);
    argument_length = wcslen(argument);
    if (argument_length == 0u || wcschr(argument, L' ') != NULL ||
        wcschr(argument, L'"') != NULL ||
        used + argument_length + 2u > capacity) {
        return FALSE;
    }
    command_line[used++] = L' ';
    memcpy(
        command_line + used,
        argument,
        (argument_length + 1u) * sizeof(*argument)
    );
    return TRUE;
}

int wmain(int argc, wchar_t **argv) {
    wchar_t launcher_path[MAX_PATH];
    wchar_t launcher_directory[MAX_PATH];
    wchar_t default_game[MAX_PATH];
    wchar_t default_dll[MAX_PATH];
    wchar_t game_path[MAX_PATH];
    wchar_t dll_path[MAX_PATH];
    wchar_t game_directory[MAX_PATH];
    wchar_t command_line[SUDEKIMP_MAX_COMMAND_LINE];
    SudekiMpBuildCheck build;
    STARTUPINFOW startup;
    PROCESS_INFORMATION process;
    const wchar_t *game_input;
    const wchar_t *dll_input;
    BOOL check_only;
    int first_game_argument;
    int argument_index;
    BOOL launched = FALSE;
    BOOL resumed = FALSE;
    DWORD wait_result;
    DWORD game_exit_code;
    int result = 1;

    if (GetModuleFileNameW(NULL, launcher_path, MAX_PATH) == 0) {
        print_error(L"GetModuleFileNameW");
        return 1;
    }
    directory_from_path(launcher_path, launcher_directory);

    _snwprintf(default_game, MAX_PATH, L"%ls\\SUDEKI.exe", launcher_directory);
    _snwprintf(default_dll, MAX_PATH, L"%ls\\SudekiMP.dll", launcher_directory);
    default_game[MAX_PATH - 1] = L'\0';
    default_dll[MAX_PATH - 1] = L'\0';

    check_only = argc >= 2 && lstrcmpiW(argv[1], L"--check") == 0;
    if (check_only) {
        game_input = argc >= 3 ? argv[2] : default_game;
        dll_input = argc >= 4 ? argv[3] : default_dll;
        first_game_argument = 4;
    } else {
        game_input = argc >= 2 ? argv[1] : default_game;
        dll_input = argc >= 3 ? argv[2] : default_dll;
        first_game_argument = 3;
    }

    if (!absolute_path(game_input, game_path) ||
        !absolute_path(dll_input, dll_path)) {
        fwprintf(stderr, L"SudekiMP launcher: path is invalid or too long.\n");
        return 1;
    }

    if (!SudekiMpCheckExecutableFile(game_path, &build)) {
        print_error(L"hash game executable");
        return 1;
    }
    wprintf(L"Game SHA256: %hs\n", build.actual_sha256);
    if (!build.hash_matches || !build.pe_matches) {
        fwprintf(stderr, L"SudekiMP launcher: unsupported SUDEKI.exe; refusing to inject.\n");
        return 2;
    }
    if (GetFileAttributesW(dll_path) == INVALID_FILE_ATTRIBUTES) {
        print_error(L"locate SudekiMP.dll");
        return 1;
    }
    if (check_only) {
        wprintf(L"Build supported; launcher will permit injection.\n");
        return 0;
    }

    directory_from_path(game_path, game_directory);
    _snwprintf(
        command_line,
        SUDEKIMP_MAX_COMMAND_LINE,
        L"\"%ls\"",
        game_path
    );
    command_line[SUDEKIMP_MAX_COMMAND_LINE - 1u] = L'\0';
    for (argument_index = first_game_argument;
         argument_index < argc;
         ++argument_index) {
        if (!append_command_line_argument(
                command_line,
                SUDEKIMP_MAX_COMMAND_LINE,
                argv[argument_index])) {
            fwprintf(
                stderr,
                L"SudekiMP launcher: game arguments must be non-empty "
                L"tokens without spaces or quotes.\n"
            );
            return 1;
        }
    }

    ZeroMemory(&startup, sizeof(startup));
    startup.cb = sizeof(startup);
    ZeroMemory(&process, sizeof(process));

    if (!CreateProcessW(
            game_path,
            command_line,
            NULL,
            NULL,
            FALSE,
            CREATE_SUSPENDED,
            NULL,
            game_directory,
            &startup,
            &process)) {
        print_error(L"CreateProcessW");
        return 1;
    }
    launched = TRUE;

    if (!inject_and_initialize(&process, dll_path)) {
        fwprintf(stderr, L"SudekiMP launcher: injection failed; game was not resumed.\n");
        goto cleanup;
    }
    if (ResumeThread(process.hThread) == (DWORD)-1) {
        print_error(L"ResumeThread");
        goto cleanup;
    }
    resumed = TRUE;

    wprintf(L"SudekiMP loaded successfully.\n");
    fflush(stdout);
    wait_result = WaitForSingleObject(process.hProcess, INFINITE);
    if (wait_result != WAIT_OBJECT_0) {
        print_error(L"wait for Sudeki");
        goto cleanup;
    }
    if (GetExitCodeProcess(process.hProcess, &game_exit_code)) {
        wprintf(L"Sudeki exited with code %lu.\n",
            (unsigned long)game_exit_code);
    }
    result = 0;

cleanup:
    if (result != 0 && launched && !resumed) {
        TerminateProcess(process.hProcess, (UINT)result);
    }
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    return result;
}
