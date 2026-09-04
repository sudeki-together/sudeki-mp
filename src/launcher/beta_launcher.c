#define _WIN32_WINNT 0x0600

#include <windows.h>
#include <commctrl.h>
#include <mmsystem.h>
#include <shellapi.h>
#include <shlobj.h>
#include <strsafe.h>
#include <tlhelp32.h>
#include <urlmon.h>

#include <string.h>
#include <wchar.h>

#include "beta_launcher_resource.h"

#define SUDEKIMP_TITLE L"SudekiMP Launcher"
#define SUDEKIMP_LAUNCHER_VERSION L"0.4.0"
#define SUDEKIMP_PROJECT_URL L"https://git.unfilteredrealm.com/wander"
#define SUDEKIMP_WINDOWS_BETA_URL \
    L"https://git.unfilteredrealm.com/sudeki-together/-/packages/generic/sudekimp-windows-beta"
#define SUDEKIMP_MUSIC_MANIFEST_URL \
    L"https://git.unfilteredrealm.com/sudeki-together/sudeki-mp/raw/branch/main/public/music/manifest.txt"
#define SUDEKIMP_MUSIC_TRACK_URL \
    L"https://git.unfilteredrealm.com/sudeki-together/sudeki-mp/raw/branch/main/public/music/Map%20Inversion.mp3"
#define SUDEKIMP_UPDATE_MANIFEST_URL \
    L"https://git.unfilteredrealm.com/sudeki-together/sudeki-mp/raw/branch/main/public/launcher-manifest.txt"

static HINSTANCE launcher_instance;
static HWND launcher_window;
static HWND directory_edit;
static HWND status_label;
static HWND profile_combo;
static HWND lan_host_edit;
static HWND lan_port_edit;
static HWND auto_update_checkbox;
static HWND cleanroom_tools_checkbox;
static WCHAR package_directory[MAX_PATH];
static WCHAR music_cache_path[MAX_PATH];
static LONG music_download_running;
static HANDLE launched_game_job;
static HBRUSH app_background_brush;
static HBRUSH panel_background_brush;
static HBRUSH input_background_brush;
static HFONT body_font;
static HFONT title_font;
static HFONT subtitle_font;

typedef enum SudekiMpLauncherProfile {
    SUDEKIMP_PROFILE_LOCAL_COOP = 0,
    SUDEKIMP_PROFILE_LAN_HOST = 1,
    SUDEKIMP_PROFILE_LAN_CLIENT = 2,
    SUDEKIMP_PROFILE_CLEANROOM = 3,
    SUDEKIMP_PROFILE_SAFE = 4
} SudekiMpLauncherProfile;

#define SUDEKIMP_COLOR_BACKGROUND RGB(12, 20, 31)
#define SUDEKIMP_COLOR_PANEL RGB(23, 34, 49)
#define SUDEKIMP_COLOR_INPUT RGB(17, 27, 40)
#define SUDEKIMP_COLOR_TEXT RGB(232, 240, 248)
#define SUDEKIMP_COLOR_MUTED RGB(159, 181, 202)
#define SUDEKIMP_COLOR_CYAN RGB(54, 193, 218)
#define SUDEKIMP_COLOR_BLUE RGB(40, 100, 168)
#define SUDEKIMP_COLOR_BUTTON RGB(38, 55, 75)

static void set_status(const WCHAR *text) {
    if (status_label != NULL) {
        SetWindowTextW(status_label, text);
    }
}

static void show_error(HWND owner, const WCHAR *text) {
    MessageBoxW(owner, text, SUDEKIMP_TITLE, MB_OK | MB_ICONERROR);
}

static BOOL file_exists(const WCHAR *path) {
    const DWORD attributes = GetFileAttributesW(path);
    return attributes != INVALID_FILE_ATTRIBUTES &&
           (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

static BOOL directory_exists(const WCHAR *path) {
    const DWORD attributes = GetFileAttributesW(path);
    return attributes != INVALID_FILE_ATTRIBUTES &&
           (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

static BOOL join_path(WCHAR *destination,
                      size_t destination_count,
                      const WCHAR *directory,
                      const WCHAR *leaf) {
    return SUCCEEDED(StringCchPrintfW(destination,
                                      destination_count,
                                      L"%s\\%s",
                                      directory,
                                      leaf));
}

static void trim_directory(WCHAR *value) {
    WCHAR *start = value;
    size_t length;

    while (*start == L' ' || *start == L'\t' || *start == L'\"') {
        ++start;
    }
    if (start != value) {
        MoveMemory(value, start, (lstrlenW(start) + 1u) * sizeof(*value));
    }

    length = lstrlenW(value);
    while (length > 0u &&
           (value[length - 1u] == L' ' || value[length - 1u] == L'\t' ||
            value[length - 1u] == L'\"' || value[length - 1u] == L'\\')) {
        value[--length] = L'\0';
    }
}

static BOOL initialise_package_directory(void) {
    DWORD length = GetModuleFileNameW(NULL,
                                      package_directory,
                                      (DWORD)(sizeof(package_directory) /
                                              sizeof(package_directory[0])));
    WCHAR *last_separator;

    if (length == 0u || length >= MAX_PATH) {
        return FALSE;
    }
    last_separator = wcsrchr(package_directory, L'\\');
    if (last_separator == NULL) {
        return FALSE;
    }
    *last_separator = L'\0';
    return TRUE;
}

static BOOL get_settings_directory(WCHAR *destination, size_t destination_count) {
    WCHAR local_app_data[MAX_PATH];
    const DWORD length = GetEnvironmentVariableW(L"LOCALAPPDATA",
                                                  local_app_data,
                                                  (DWORD)(sizeof(local_app_data) /
                                                          sizeof(local_app_data[0])));
    if (length == 0u || length >= MAX_PATH) {
        return FALSE;
    }
    if (!directory_exists(local_app_data) && !CreateDirectoryW(local_app_data, NULL)) {
        return FALSE;
    }
    return join_path(destination, destination_count, local_app_data, L"SudekiMP");
}

static BOOL get_settings_path(WCHAR *destination, size_t destination_count) {
    WCHAR directory[MAX_PATH];
    if (!get_settings_directory(directory, sizeof(directory) / sizeof(directory[0]))) {
        return FALSE;
    }
    if (!directory_exists(directory) && !CreateDirectoryW(directory, NULL)) {
        return FALSE;
    }
    return join_path(destination, destination_count, directory, L"windows-beta-launcher.ini");
}

static BOOL get_music_cache_path(WCHAR *destination, size_t destination_count) {
    WCHAR settings_directory[MAX_PATH];
    WCHAR music_directory[MAX_PATH];
    if (!get_settings_directory(settings_directory,
                                sizeof(settings_directory) /
                                    sizeof(settings_directory[0]))) {
        return FALSE;
    }
    if (!directory_exists(settings_directory) &&
        !CreateDirectoryW(settings_directory, NULL)) {
        return FALSE;
    }
    if (!join_path(music_directory,
                   sizeof(music_directory) / sizeof(music_directory[0]),
                   settings_directory,
                   L"music")) {
        return FALSE;
    }
    if (!directory_exists(music_directory) && !CreateDirectoryW(music_directory, NULL)) {
        return FALSE;
    }
    return join_path(destination, destination_count, music_directory, L"Map Inversion.mp3");
}

static BOOL selected_game_directory(WCHAR *destination, size_t destination_count) {
    WCHAR game_executable[MAX_PATH];
    GetWindowTextW(directory_edit, destination, (int)destination_count);
    trim_directory(destination);
    if (destination[0] == L'\0' ||
        !join_path(game_executable,
                   sizeof(game_executable) / sizeof(game_executable[0]),
                   destination,
                   L"SUDEKI.exe") ||
        !file_exists(game_executable)) {
        return FALSE;
    }
    return TRUE;
}

static void persist_game_directory(const WCHAR *game_directory) {
    WCHAR settings_path[MAX_PATH];
    if (get_settings_path(settings_path, sizeof(settings_path) / sizeof(settings_path[0]))) {
        WritePrivateProfileStringW(L"launcher",
                                   L"game_directory",
                                   game_directory,
                                   settings_path);
    }
}

static void persist_launcher_options(void) {
    WCHAR settings_path[MAX_PATH];
    WCHAR value[64];
    int profile;
    if (!get_settings_path(settings_path,
            sizeof(settings_path) / sizeof(settings_path[0]))) return;
    profile = profile_combo == NULL ? SUDEKIMP_PROFILE_LOCAL_COOP :
        (int)SendMessageW(profile_combo, CB_GETCURSEL, 0, 0);
    StringCchPrintfW(value, sizeof(value) / sizeof(value[0]), L"%d", profile);
    WritePrivateProfileStringW(L"launcher", L"profile", value, settings_path);
    if (lan_host_edit != NULL) {
        GetWindowTextW(lan_host_edit, value,
            (int)(sizeof(value) / sizeof(value[0])));
        WritePrivateProfileStringW(L"launcher", L"lan_host", value, settings_path);
    }
    if (lan_port_edit != NULL) {
        GetWindowTextW(lan_port_edit, value,
            (int)(sizeof(value) / sizeof(value[0])));
        WritePrivateProfileStringW(L"launcher", L"lan_port", value, settings_path);
    }
    WritePrivateProfileStringW(L"launcher", L"check_updates_on_startup",
        auto_update_checkbox != NULL &&
            SendMessageW(auto_update_checkbox, BM_GETCHECK, 0, 0) == BST_CHECKED ?
            L"true" : L"false", settings_path);
    WritePrivateProfileStringW(L"launcher", L"cleanroom_tools",
        cleanroom_tools_checkbox != NULL &&
            SendMessageW(cleanroom_tools_checkbox, BM_GETCHECK, 0, 0) == BST_CHECKED ?
            L"true" : L"false", settings_path);
}

static void load_saved_game_directory(void) {
    WCHAR settings_path[MAX_PATH];
    WCHAR saved_directory[MAX_PATH];
    if (!get_settings_path(settings_path, sizeof(settings_path) / sizeof(settings_path[0]))) {
        return;
    }
    GetPrivateProfileStringW(L"launcher",
                             L"game_directory",
                             L"",
                             saved_directory,
                             (DWORD)(sizeof(saved_directory) / sizeof(saved_directory[0])),
                             settings_path);
    if (saved_directory[0] != L'\0') {
        SetWindowTextW(directory_edit, saved_directory);
    }
    if (profile_combo != NULL) {
        int profile = GetPrivateProfileIntW(L"launcher", L"profile",
            SUDEKIMP_PROFILE_LOCAL_COOP, settings_path);
        if (profile < SUDEKIMP_PROFILE_LOCAL_COOP || profile > SUDEKIMP_PROFILE_SAFE) {
            profile = SUDEKIMP_PROFILE_LOCAL_COOP;
        }
        SendMessageW(profile_combo, CB_SETCURSEL, (WPARAM)profile, 0);
    }
    if (lan_host_edit != NULL) {
        GetPrivateProfileStringW(L"launcher", L"lan_host", L"127.0.0.1",
            saved_directory, (DWORD)(sizeof(saved_directory) /
                sizeof(saved_directory[0])), settings_path);
        SetWindowTextW(lan_host_edit, saved_directory);
    }
    if (lan_port_edit != NULL) {
        GetPrivateProfileStringW(L"launcher", L"lan_port", L"26770",
            saved_directory, (DWORD)(sizeof(saved_directory) /
                sizeof(saved_directory[0])), settings_path);
        SetWindowTextW(lan_port_edit, saved_directory);
    }
    if (auto_update_checkbox != NULL) {
        GetPrivateProfileStringW(L"launcher", L"check_updates_on_startup", L"false",
            saved_directory, (DWORD)(sizeof(saved_directory) /
                sizeof(saved_directory[0])), settings_path);
        SendMessageW(auto_update_checkbox, BM_SETCHECK,
            lstrcmpiW(saved_directory, L"true") == 0 ? BST_CHECKED : BST_UNCHECKED, 0);
    }
    if (cleanroom_tools_checkbox != NULL) {
        GetPrivateProfileStringW(L"launcher", L"cleanroom_tools", L"true",
            saved_directory, (DWORD)(sizeof(saved_directory) /
                sizeof(saved_directory[0])), settings_path);
        SendMessageW(cleanroom_tools_checkbox, BM_SETCHECK,
            lstrcmpiW(saved_directory, L"false") == 0 ? BST_UNCHECKED : BST_CHECKED, 0);
    }
}

static BOOL validate_install(HWND owner,
                             WCHAR *game_directory,
                             WCHAR *loader_path,
                             WCHAR *dll_path) {
    WCHAR game_executable[MAX_PATH];

    if (!selected_game_directory(game_directory, MAX_PATH)) {
        show_error(owner, L"Paste or choose the folder that contains SUDEKI.exe.");
        return FALSE;
    }
    if (!join_path(game_executable, MAX_PATH, game_directory, L"SUDEKI.exe") ||
        !join_path(loader_path, MAX_PATH, package_directory, L"SudekiMP.Launcher.exe") ||
        !join_path(dll_path, MAX_PATH, package_directory, L"SudekiMP.dll")) {
        show_error(owner, L"One of the launcher paths is too long.");
        return FALSE;
    }
    if (!file_exists(loader_path) || !file_exists(dll_path)) {
        show_error(owner,
                   L"This SudekiMP folder is incomplete. Reinstall the Windows beta package.");
        return FALSE;
    }
    persist_game_directory(game_directory);
    return TRUE;
}

static BOOL build_loader_command(WCHAR *command,
                                 size_t command_count,
                                 const WCHAR *loader_path,
                                 const WCHAR *game_directory,
                                 const WCHAR *dll_path,
                                 BOOL check_only,
                                 SudekiMpLauncherProfile profile) {
    WCHAR game_executable[MAX_PATH];
    const WCHAR *game_arguments = L"";
    if (!join_path(game_executable, MAX_PATH, game_directory, L"SUDEKI.exe")) {
        return FALSE;
    }
    if (!check_only) {
        if (profile == SUDEKIMP_PROFILE_LAN_HOST) {
            game_arguments = L" --game-arg=-Level --game-arg=testroom "
                L"--game-arg=-DT --game-arg=1 --game-arg=-Tal --game-arg=1";
        } else if (profile == SUDEKIMP_PROFILE_LAN_CLIENT ||
                   profile == SUDEKIMP_PROFILE_CLEANROOM) {
            game_arguments = L" --game-arg=-Level --game-arg=testroom "
                L"--game-arg=-DT --game-arg=1 --game-arg=-Ailish --game-arg=1";
        }
    }
    return SUCCEEDED(StringCchPrintfW(command,
                                      command_count,
                                      check_only
                                          ? L"\"%s\" --check \"%s\" \"%s\""
                                          : L"\"%s\" \"%s\" \"%s\"%s",
                                      loader_path,
                                      game_executable,
                                      dll_path,
                                      game_arguments));
}

static BOOL verify_game(HWND owner,
                        WCHAR game_directory[MAX_PATH],
                        WCHAR loader_path[MAX_PATH],
                        WCHAR dll_path[MAX_PATH]) {
    WCHAR command[MAX_PATH * 3u + 80u];
    STARTUPINFOW startup;
    PROCESS_INFORMATION process;
    DWORD exit_code = 1u;

    if (!validate_install(owner, game_directory, loader_path, dll_path) ||
        !build_loader_command(command,
                              sizeof(command) / sizeof(command[0]),
                              loader_path,
                              game_directory,
                              dll_path,
                              TRUE,
                              SUDEKIMP_PROFILE_SAFE)) {
        return FALSE;
    }
    ZeroMemory(&startup, sizeof(startup));
    ZeroMemory(&process, sizeof(process));
    startup.cb = sizeof(startup);
    set_status(L"Verifying the selected GOG game build…");
    RedrawWindow(launcher_window, NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW);
    if (launched_game_job != NULL) {
        CloseHandle(launched_game_job);
        launched_game_job = NULL;
    }
    launched_game_job = CreateJobObjectW(NULL, NULL);
    if (launched_game_job == NULL || !CreateProcessW(NULL,
                        command,
                        NULL,
                        NULL,
                        FALSE,
                        CREATE_NO_WINDOW,
                        NULL,
                        game_directory,
                        &startup,
                        &process)) {
        show_error(owner, L"SudekiMP could not start its verification loader.");
        set_status(L"Verification could not start.");
        return FALSE;
    }
    WaitForSingleObject(process.hProcess, INFINITE);
    GetExitCodeProcess(process.hProcess, &exit_code);
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    if (exit_code != 0u) {
        show_error(owner,
                   L"This is not the supported GOG SUDEKI.exe build. SudekiMP did not launch it.");
        set_status(L"Verification failed. Launch blocked.");
        return FALSE;
    }
    set_status(L"Verification passed. This exact GOG build is supported.");
    return TRUE;
}

static BOOL disable_all_optional_profiles(const WCHAR *config_path) {
    enum { SECTION_CAPACITY = 32768u };
    WCHAR *section;
    WCHAR *entry;
    DWORD length;
    BOOL success = TRUE;
    section = (WCHAR *)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY,
        SECTION_CAPACITY * sizeof(WCHAR));
    if (section == NULL) return FALSE;
    length = GetPrivateProfileSectionW(
        L"SudekiMP", section, SECTION_CAPACITY, config_path);
    if (length >= SECTION_CAPACITY - 2u) success = FALSE;
    entry = section;
    while (success && *entry != L'\0') {
        WCHAR *separator = wcschr(entry, L'=');
        WCHAR *next = entry + lstrlenW(entry) + 1u;
        if (separator != NULL) {
            *separator = L'\0';
            if (wcsncmp(entry, L"Enable", 6u) == 0 &&
                !WritePrivateProfileStringW(
                    L"SudekiMP", entry, L"false", config_path)) {
                success = FALSE;
            }
        }
        entry = next;
    }
    HeapFree(GetProcessHeap(), 0u, section);
    return success;
}

static BOOL configure_launcher_profile(
    HWND owner,
    SudekiMpLauncherProfile profile
) {
    static const WCHAR *const coop_keys[] = {
        L"EnableCoopRosterMenu",
        L"EnableControlSeparationPrototype",
        L"EnableSecondPlayerMovementPrototype",
        L"EnableSecondPlayerCameraRelativeMovementPrototype",
        L"EnableSecondPlayerSeparationGuardPrototype",
        L"EnableSecondPlayerWeakAttackPrototype",
        L"EnableNativeXInputPlayerTwoPrototype",
        L"EnableSplitScreenRenderPrototype",
        L"EnableSecondPlayerCameraPrototype",
        L"EnableDualCameraFrameCachePrototype",
        L"EnableSecondPlayerControllerCameraPrototype",
        L"EnablePartyAtomicTransitionsPrototype"
    };
    WCHAR config_path[MAX_PATH];
    WCHAR lan_host[64];
    WCHAR lan_port[16];
    size_t index;

    if (!join_path(config_path,
                   sizeof(config_path) / sizeof(config_path[0]),
                   package_directory,
                   L"SudekiMP.ini") || !file_exists(config_path)) {
        show_error(owner,
                   L"This beta package is missing SudekiMP.ini. Reinstall it before launching.");
        return FALSE;
    }
    if (!disable_all_optional_profiles(config_path)) {
        show_error(owner,
                   L"SudekiMP could not reset the package to a closed launch profile.");
        return FALSE;
    }
    if (profile == SUDEKIMP_PROFILE_LOCAL_COOP) {
        for (index = 0u; index < sizeof(coop_keys) / sizeof(coop_keys[0]); ++index) {
            if (!WritePrivateProfileStringW(
                    L"SudekiMP", coop_keys[index], L"true", config_path)) {
                show_error(owner,
                           L"SudekiMP could not write its local co-op profile.");
                return FALSE;
            }
        }
        if (!WritePrivateProfileStringW(L"SudekiMP", L"XInputPlayerTwoSlot", L"0",
                                         config_path) ||
            !WritePrivateProfileStringW(L"Bindings", L"ToggleSecondPlayerAi", L"F10",
                                         config_path)) {
            show_error(owner,
                       L"SudekiMP could not finish its local co-op settings.");
            return FALSE;
        }
    } else if (profile == SUDEKIMP_PROFILE_LAN_HOST ||
               profile == SUDEKIMP_PROFILE_LAN_CLIENT) {
        GetWindowTextW(lan_host_edit, lan_host,
            (int)(sizeof(lan_host) / sizeof(lan_host[0])));
        GetWindowTextW(lan_port_edit, lan_port,
            (int)(sizeof(lan_port) / sizeof(lan_port[0])));
        if (lan_host[0] == L'\0' || lan_port[0] == L'\0' ||
            !WritePrivateProfileStringW(L"SudekiMP", L"LanArenaHost", lan_host,
                                         config_path) ||
            !WritePrivateProfileStringW(L"SudekiMP", L"LanArenaPort", lan_port,
                                         config_path) ||
            !WritePrivateProfileStringW(L"SudekiMP", L"SkipStartupMovies", L"true",
                                         config_path) ||
            !WritePrivateProfileStringW(L"SudekiMP", L"EnableControlSeparationPrototype",
                                         L"true", config_path) ||
            !WritePrivateProfileStringW(L"SudekiMP", L"EnableCleanroomMenu",
                profile == SUDEKIMP_PROFILE_LAN_HOST ? L"true" : L"false",
                config_path) ||
            !WritePrivateProfileStringW(L"SudekiMP",
                profile == SUDEKIMP_PROFILE_LAN_HOST ?
                    L"EnableLanArenaHostPrototype" :
                    L"EnableLanArenaClientPrototype",
                L"true", config_path)) {
            show_error(owner, L"SudekiMP could not write its closed LAN arena profile.");
            return FALSE;
        }
    } else if (profile == SUDEKIMP_PROFILE_CLEANROOM) {
        const BOOL cleanroom_tools_enabled = cleanroom_tools_checkbox == NULL ||
            SendMessageW(cleanroom_tools_checkbox, BM_GETCHECK, 0, 0) == BST_CHECKED;
        if (!WritePrivateProfileStringW(
                L"SudekiMP", L"SkipStartupMovies", L"true", config_path) ||
            !WritePrivateProfileStringW(
                L"SudekiMP", L"EnableCleanroomMenu",
                cleanroom_tools_enabled ? L"true" : L"false", config_path)) {
            show_error(owner, L"SudekiMP could not write the cleanroom profile.");
            return FALSE;
        }
    }
    /* Flush the profile cache before the injected DLL reads the file. */
    WritePrivateProfileStringW(NULL, NULL, NULL, config_path);
    return TRUE;
}

static void launch_game(HWND owner) {
    WCHAR game_directory[MAX_PATH];
    WCHAR loader_path[MAX_PATH];
    WCHAR dll_path[MAX_PATH];
    WCHAR command[MAX_PATH * 3u + 80u];
    STARTUPINFOW startup;
    PROCESS_INFORMATION process;
    int selected_profile = profile_combo == NULL ? SUDEKIMP_PROFILE_LOCAL_COOP :
        (int)SendMessageW(profile_combo, CB_GETCURSEL, 0, 0);
    SudekiMpLauncherProfile profile;

    if (selected_profile < SUDEKIMP_PROFILE_LOCAL_COOP ||
        selected_profile > SUDEKIMP_PROFILE_SAFE) {
        selected_profile = SUDEKIMP_PROFILE_LOCAL_COOP;
    }
    profile = (SudekiMpLauncherProfile)selected_profile;

    persist_launcher_options();
    if (!configure_launcher_profile(owner, profile) ||
        !verify_game(owner, game_directory, loader_path, dll_path) ||
        !build_loader_command(command,
                              sizeof(command) / sizeof(command[0]),
                              loader_path,
                              game_directory,
                              dll_path,
                              FALSE,
                              profile)) {
        return;
    }
    ZeroMemory(&startup, sizeof(startup));
    ZeroMemory(&process, sizeof(process));
    startup.cb = sizeof(startup);
    if (!CreateProcessW(NULL,
                        command,
                        NULL,
                        NULL,
                        FALSE,
                        CREATE_NEW_CONSOLE | CREATE_SUSPENDED,
                        NULL,
                        game_directory,
                        &startup,
                        &process)) {
        show_error(owner, L"SudekiMP could not start the loader.");
        set_status(L"Launch failed before injection.");
        if (launched_game_job != NULL) CloseHandle(launched_game_job);
        launched_game_job = NULL;
        return;
    }
    if (!AssignProcessToJobObject(launched_game_job, process.hProcess)) {
        TerminateProcess(process.hProcess, 1u);
        CloseHandle(process.hThread);
        CloseHandle(process.hProcess);
        CloseHandle(launched_game_job);
        launched_game_job = NULL;
        show_error(owner, L"SudekiMP could not create a safely tracked launch session.");
        set_status(L"Launch blocked before the game started.");
        return;
    }
    ResumeThread(process.hThread);
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    if (profile == SUDEKIMP_PROFILE_LAN_HOST) {
        set_status(L"LAN arena host started as Tal. Keep the loader console for errors.");
    } else if (profile == SUDEKIMP_PROFILE_LAN_CLIENT) {
        set_status(L"LAN arena client started as Ailish. Keep the loader console for errors.");
    } else if (profile == SUDEKIMP_PROFILE_CLEANROOM) {
        set_status(cleanroom_tools_checkbox != NULL &&
                SendMessageW(cleanroom_tools_checkbox, BM_GETCHECK, 0, 0) == BST_CHECKED ?
            L"Cleanroom started. Press F8 for sandbox tools; campaign saves are not used." :
            L"Cleanroom started with F8 sandbox tools disabled; campaign saves are not used.");
    } else if (profile == SUDEKIMP_PROFILE_LOCAL_COOP) {
        set_status(L"Windows local co-op started. Player 2 uses XInput slot 0.");
    } else {
        set_status(L"Safe SudekiMP launch started.");
    }
}

static void browse_for_game_directory(HWND owner) {
    BROWSEINFOW browse;
    PIDLIST_ABSOLUTE item;
    WCHAR candidate[MAX_PATH];

    ZeroMemory(&browse, sizeof(browse));
    browse.hwndOwner = owner;
    browse.lpszTitle = L"Select the folder that contains SUDEKI.exe";
    browse.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    item = SHBrowseForFolderW(&browse);
    if (item == NULL) {
        return;
    }
    if (SHGetPathFromIDListW(item, candidate)) {
        WCHAR executable[MAX_PATH];
        if (join_path(executable,
                      sizeof(executable) / sizeof(executable[0]),
                      candidate,
                      L"SUDEKI.exe") &&
            file_exists(executable)) {
            SetWindowTextW(directory_edit, candidate);
            persist_game_directory(candidate);
            set_status(L"Sudeki folder saved. Verify it before launching.");
        } else {
            show_error(owner, L"That folder does not contain SUDEKI.exe.");
        }
    }
    CoTaskMemFree(item);
}

static BOOL create_directory_if_missing(const WCHAR *path) {
    const DWORD attributes = GetFileAttributesW(path);

    if (attributes != INVALID_FILE_ATTRIBUTES) {
        return (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
    }
    return CreateDirectoryW(path, NULL) != 0 || GetLastError() == ERROR_ALREADY_EXISTS;
}

static BOOL remove_directory_tree(const WCHAR *path) {
    WCHAR pattern[MAX_PATH];
    WIN32_FIND_DATAW entry;
    HANDLE search;

    if (!join_path(pattern, MAX_PATH, path, L"*")) {
        return FALSE;
    }
    search = FindFirstFileW(pattern, &entry);
    if (search != INVALID_HANDLE_VALUE) {
        do {
            WCHAR child[MAX_PATH];
            if (lstrcmpW(entry.cFileName, L".") == 0 ||
                lstrcmpW(entry.cFileName, L"..") == 0) {
                continue;
            }
            if (!join_path(child, MAX_PATH, path, entry.cFileName)) {
                FindClose(search);
                return FALSE;
            }
            if ((entry.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
                if (!remove_directory_tree(child)) {
                    FindClose(search);
                    return FALSE;
                }
            } else if (DeleteFileW(child) == 0) {
                FindClose(search);
                return FALSE;
            }
        } while (FindNextFileW(search, &entry));
        FindClose(search);
    }
    return RemoveDirectoryW(path) != 0 || GetLastError() == ERROR_FILE_NOT_FOUND;
}

static BOOL copy_directory_tree(const WCHAR *source, const WCHAR *destination) {
    WCHAR pattern[MAX_PATH];
    WIN32_FIND_DATAW entry;
    HANDLE search;

    if (!create_directory_if_missing(destination) ||
        !join_path(pattern, MAX_PATH, source, L"*")) {
        return FALSE;
    }
    search = FindFirstFileW(pattern, &entry);
    if (search == INVALID_HANDLE_VALUE) {
        return FALSE;
    }
    do {
        WCHAR source_child[MAX_PATH];
        WCHAR destination_child[MAX_PATH];
        if (lstrcmpW(entry.cFileName, L".") == 0 ||
            lstrcmpW(entry.cFileName, L"..") == 0) {
            continue;
        }
        if (!join_path(source_child, MAX_PATH, source, entry.cFileName) ||
            !join_path(destination_child, MAX_PATH, destination, entry.cFileName)) {
            FindClose(search);
            return FALSE;
        }
        if ((entry.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
            if (!copy_directory_tree(source_child, destination_child)) {
                FindClose(search);
                return FALSE;
            }
        } else if (CopyFileW(source_child, destination_child, TRUE) == 0) {
            FindClose(search);
            return FALSE;
        }
    } while (FindNextFileW(search, &entry));
    FindClose(search);
    return TRUE;
}

static BOOL get_sudeki_save_directory(WCHAR save_directory[MAX_PATH],
                                      WCHAR sudeki_directory[MAX_PATH]) {
    WCHAR app_data[MAX_PATH];

    if (FAILED(SHGetFolderPathW(NULL,
                                CSIDL_APPDATA | CSIDL_FLAG_CREATE,
                                NULL,
                                SHGFP_TYPE_CURRENT,
                                app_data)) ||
        !join_path(sudeki_directory, MAX_PATH, app_data, L"Sudeki") ||
        !create_directory_if_missing(sudeki_directory) ||
        !join_path(save_directory, MAX_PATH, sudeki_directory, L"Save")) {
        return FALSE;
    }
    return TRUE;
}

static BOOL install_coop_save_fixtures(HWND owner) {
    WCHAR fixture_root[MAX_PATH];
    WCHAR fixture_marker[MAX_PATH];
    WCHAR save_directory[MAX_PATH];
    WCHAR sudeki_directory[MAX_PATH];
    WCHAR backup_root[MAX_PATH];
    WCHAR backup_directory[MAX_PATH];
    SYSTEMTIME now;
    DWORD save_attributes;
    BOOL moved_existing_save = FALSE;
    INT_PTR confirmation;

    if (!join_path(fixture_root,
                   MAX_PATH,
                   package_directory,
                   L"CoopSaveFixtures") ||
        !join_path(fixture_marker,
                   MAX_PATH,
                   fixture_root,
                   L"SAVESLOT0000\\sudeki.fish") ||
        !file_exists(fixture_marker)) {
        show_error(owner,
                   L"This beta package does not contain the co-op save fixtures. "
                   L"Download the current Windows beta package.");
        return FALSE;
    }
    if (!get_sudeki_save_directory(save_directory, sudeki_directory)) {
        show_error(owner, L"Windows could not locate the Sudeki save directory.");
        return FALSE;
    }
    confirmation = MessageBoxW(
        owner,
        L"WARNING: This will move your current Sudeki saves out of the live save "
        L"location and install the SudekiMP co-op test saves.\n\n"
        L"Your old saves are moved to %APPDATA%\\Sudeki\\SudekiMP-Backups first. "
        L"They are not deleted, but the game will no longer see them until you "
        L"restore that folder manually.\n\nContinue?",
        L"Install co-op save fixtures",
        MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2);
    if (confirmation != IDYES) {
        set_status(L"Co-op save installation cancelled. Existing saves were untouched.");
        return FALSE;
    }

    save_attributes = GetFileAttributesW(save_directory);
    if (save_attributes != INVALID_FILE_ATTRIBUTES &&
        (save_attributes & FILE_ATTRIBUTE_DIRECTORY) == 0) {
        show_error(owner, L"The Sudeki save path is a file, not a folder. No files changed.");
        return FALSE;
    }
    if (!join_path(backup_root, MAX_PATH, sudeki_directory, L"SudekiMP-Backups") ||
        !create_directory_if_missing(backup_root)) {
        show_error(owner, L"Windows could not create the SudekiMP save-backup folder.");
        return FALSE;
    }
    GetLocalTime(&now);
    if (FAILED(StringCchPrintfW(backup_directory,
                                MAX_PATH,
                                L"%s\\Save-%04u%02u%02u-%02u%02u%02u",
                                backup_root,
                                (unsigned int)now.wYear,
                                (unsigned int)now.wMonth,
                                (unsigned int)now.wDay,
                                (unsigned int)now.wHour,
                                (unsigned int)now.wMinute,
                                (unsigned int)now.wSecond))) {
        show_error(owner, L"The save-backup path is too long. No files changed.");
        return FALSE;
    }
    if (save_attributes != INVALID_FILE_ATTRIBUTES) {
        if (MoveFileExW(save_directory, backup_directory, MOVEFILE_WRITE_THROUGH) == 0) {
            show_error(owner, L"Windows could not archive the existing Sudeki saves. No files changed.");
            return FALSE;
        }
        moved_existing_save = TRUE;
    }
    if (!create_directory_if_missing(save_directory) ||
        !copy_directory_tree(fixture_root, save_directory)) {
        (void)remove_directory_tree(save_directory);
        if (moved_existing_save) {
            (void)MoveFileExW(backup_directory, save_directory, MOVEFILE_WRITE_THROUGH);
        }
        show_error(owner,
                   L"Installing co-op saves failed. SudekiMP attempted to restore your old saves; "
                   L"check %APPDATA%\\Sudeki\\SudekiMP-Backups before launching the game.");
        return FALSE;
    }
    set_status(moved_existing_save ?
        L"Co-op saves installed. Your old saves are archived under %APPDATA%\\Sudeki\\SudekiMP-Backups." :
        L"Co-op saves installed. No existing Sudeki save folder needed archiving.");
    return TRUE;
}

static void test_xinput_controller(HWND owner) {
    WCHAR probe_path[MAX_PATH];
    WCHAR parameters[MAX_PATH + 32u];

    if (!join_path(probe_path,
                   MAX_PATH,
                   package_directory,
                   L"SudekiMP.XInputProbe.exe") ||
        !file_exists(probe_path) ||
        FAILED(StringCchPrintfW(parameters,
                                sizeof(parameters) / sizeof(parameters[0]),
                                L"/k \"\"%s\" & echo. & pause\"",
                                probe_path))) {
        show_error(owner, L"This beta package does not include the Windows XInput diagnostic.");
        return;
    }
    if ((INT_PTR)ShellExecuteW(owner,
                               L"open",
                               L"cmd.exe",
                               parameters,
                               package_directory,
                               SW_SHOWNORMAL) <= 32) {
        show_error(owner, L"Windows could not start the XInput diagnostic.");
    }
}

static void close_music(void) {
    mciSendStringW(L"close SudekiMPMusic", NULL, 0u, NULL);
}

static BOOL play_cached_music(HWND owner) {
    WCHAR command[MAX_PATH + 80u];
    MCIERROR result;

    if (!file_exists(music_cache_path)) {
        show_error(owner, L"The project music file is not available yet.");
        return FALSE;
    }
    close_music();
    if (FAILED(StringCchPrintfW(command,
                                sizeof(command) / sizeof(command[0]),
                                L"open \"%s\" type mpegvideo alias SudekiMPMusic",
                                music_cache_path))) {
        return FALSE;
    }
    result = mciSendStringW(command, NULL, 0u, NULL);
    if (result == 0u) {
        result = mciSendStringW(L"play SudekiMPMusic from 0", NULL, 0u, NULL);
    }
    if (result != 0u) {
        close_music();
        show_error(owner,
                   L"Windows could not play the cached MP3. The launcher itself is unaffected.");
        return FALSE;
    }
    set_status(L"Playing Map Inversion from the project music cache.");
    return TRUE;
}

static BOOL manifest_names_expected_track(const WCHAR *manifest_path) {
    HANDLE file;
    DWORD size;
    DWORD read_count = 0u;
    char buffer[512];

    file = CreateFileW(manifest_path,
                       GENERIC_READ,
                       FILE_SHARE_READ,
                       NULL,
                       OPEN_EXISTING,
                       FILE_ATTRIBUTE_NORMAL,
                       NULL);
    if (file == INVALID_HANDLE_VALUE) {
        return FALSE;
    }
    size = GetFileSize(file, NULL);
    if (size == INVALID_FILE_SIZE || size >= sizeof(buffer) ||
        !ReadFile(file, buffer, size, &read_count, NULL)) {
        CloseHandle(file);
        return FALSE;
    }
    CloseHandle(file);
    buffer[read_count] = '\0';
    return strstr(buffer, "track=Map Inversion.mp3") != NULL;
}

static DWORD WINAPI download_music_thread(void *unused) {
    WCHAR settings_directory[MAX_PATH];
    WCHAR music_directory[MAX_PATH];
    WCHAR manifest_path[MAX_PATH];
    HRESULT manifest_result;
    HRESULT track_result;
    BOOL downloaded = FALSE;
    (void)unused;

    if (get_settings_directory(settings_directory,
                               sizeof(settings_directory) / sizeof(settings_directory[0])) &&
        join_path(music_directory,
                  sizeof(music_directory) / sizeof(music_directory[0]),
                  settings_directory,
                  L"music") &&
        join_path(manifest_path,
                  sizeof(manifest_path) / sizeof(manifest_path[0]),
                  music_directory,
                  L"manifest.txt")) {
        manifest_result = URLDownloadToFileW(NULL,
                                             SUDEKIMP_MUSIC_MANIFEST_URL,
                                             manifest_path,
                                             0u,
                                             NULL);
        if (SUCCEEDED(manifest_result) && manifest_names_expected_track(manifest_path)) {
            track_result = URLDownloadToFileW(NULL,
                                              SUDEKIMP_MUSIC_TRACK_URL,
                                              music_cache_path,
                                              0u,
                                              NULL);
            downloaded = SUCCEEDED(track_result) && file_exists(music_cache_path);
        }
    }
    if (!downloaded && file_exists(music_cache_path)) {
        downloaded = TRUE;
    }
    InterlockedExchange(&music_download_running, 0);
    PostMessageW(launcher_window,
                 WM_SUDEKIMP_MUSIC_COMPLETE,
                 downloaded ? 1u : 0u,
                 0);
    return 0u;
}

static void start_music_download(HWND owner) {
    HANDLE thread;
    (void)owner;
    if (InterlockedCompareExchange(&music_download_running, 1, 0) != 0) {
        set_status(L"Project music is already being fetched from the public catalog…");
        return;
    }
    if (!get_music_cache_path(music_cache_path,
                              sizeof(music_cache_path) / sizeof(music_cache_path[0]))) {
        InterlockedExchange(&music_download_running, 0);
        show_error(launcher_window, L"SudekiMP could not create its local music cache.");
        return;
    }
    set_status(L"Fetching Map Inversion from the public project music catalog…");
    thread = CreateThread(NULL, 0u, download_music_thread, NULL, 0u, NULL);
    if (thread == NULL) {
        InterlockedExchange(&music_download_running, 0);
        show_error(launcher_window, L"SudekiMP could not start the music download.");
        return;
    }
    CloseHandle(thread);
}

static void open_windows_beta_download(HWND owner) {
    const INT_PTR result = MessageBoxW(
        owner,
        L"This opens the public SudekiMP Windows beta package page in your browser. "
        L"Download and extract the ZIP yourself, then replace only the SudekiMP "
        L"folder. It never changes SUDEKI.exe, game data, or saves.\n\nOpen the page?",
        L"Get latest SudekiMP beta",
        MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2);

    if (result != IDYES) {
        set_status(L"Download cancelled. No files changed.");
        return;
    }
    if ((INT_PTR)ShellExecuteW(owner,
                               L"open",
                               SUDEKIMP_WINDOWS_BETA_URL,
                               NULL,
                               NULL,
                               SW_SHOWNORMAL) <= 32) {
        show_error(owner, L"Windows could not open the public beta package page.");
        return;
    }
    set_status(L"Opened the public beta package page. This launcher remains unchanged.");
}

static void check_for_launcher_update(HWND owner, BOOL quiet_when_current) {
    WCHAR settings_directory[MAX_PATH];
    WCHAR manifest_path[MAX_PATH];
    HANDLE file;
    DWORD size;
    DWORD read_count = 0u;
    char buffer[2048];
    char *version;
    char *end;
    WCHAR remote_version[64];
    if (!get_settings_directory(settings_directory,
            sizeof(settings_directory) / sizeof(settings_directory[0])) ||
        !join_path(manifest_path,
            sizeof(manifest_path) / sizeof(manifest_path[0]),
            settings_directory, L"launcher-manifest.txt") ||
        FAILED(URLDownloadToFileW(NULL, SUDEKIMP_UPDATE_MANIFEST_URL,
            manifest_path, 0u, NULL))) {
        if (!quiet_when_current) {
            show_error(owner, L"The official update manifest could not be read. Nothing was changed.");
        }
        return;
    }
    file = CreateFileW(manifest_path, GENERIC_READ, FILE_SHARE_READ, NULL,
        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) return;
    size = GetFileSize(file, NULL);
    if (size == INVALID_FILE_SIZE || size >= sizeof(buffer) ||
        !ReadFile(file, buffer, size, &read_count, NULL)) {
        CloseHandle(file);
        return;
    }
    CloseHandle(file);
    buffer[read_count] = '\0';
    version = strstr(buffer, "version=");
    if (version == NULL) return;
    version += 8;
    end = strpbrk(version, "\r\n");
    if (end != NULL) *end = '\0';
    if (MultiByteToWideChar(CP_UTF8, 0, version, -1, remote_version,
            (int)(sizeof(remote_version) / sizeof(remote_version[0]))) == 0) return;
    if (lstrcmpW(remote_version, SUDEKIMP_LAUNCHER_VERSION) == 0) {
        if (!quiet_when_current) set_status(L"SudekiMP Launcher is current.");
        return;
    }
    if (MessageBoxW(owner,
            L"A different SudekiMP launcher release is available. Updates are "
            L"never installed silently. Open the official package page now?",
            L"SudekiMP update available", MB_YESNO | MB_ICONINFORMATION) == IDYES) {
        open_windows_beta_download(owner);
    }
}

static void stop_tracked_sudeki(HWND owner) {
    if (launched_game_job == NULL) {
        set_status(L"No Sudeki process started by this launcher session is tracked.");
        return;
    }
    if (MessageBoxW(owner,
            L"Stop the Sudeki process started by this launcher session? Unsaved progress will be lost.",
            L"Stop Sudeki", MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2) != IDYES) return;
    if (!TerminateJobObject(launched_game_job, 0u)) {
        show_error(owner, L"Windows could not stop the tracked Sudeki session.");
        return;
    }
    CloseHandle(launched_game_job);
    launched_game_job = NULL;
    set_status(L"The tracked Sudeki session was stopped.");
}

static BOOL copy_file_tail(
    const WCHAR *source_path,
    const WCHAR *target_path,
    DWORD maximum_bytes
) {
    HANDLE source;
    HANDLE target;
    LARGE_INTEGER size;
    LARGE_INTEGER offset;
    BYTE buffer[32768];
    DWORD read_count;
    DWORD written;
    BOOL success = TRUE;
    source = CreateFileW(source_path, GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL, NULL);
    if (source == INVALID_HANDLE_VALUE) return FALSE;
    target = CreateFileW(target_path, GENERIC_WRITE, 0u, NULL, CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL, NULL);
    if (target == INVALID_HANDLE_VALUE) {
        CloseHandle(source);
        return FALSE;
    }
    if (!GetFileSizeEx(source, &size)) {
        success = FALSE;
    } else {
        offset.QuadPart = size.QuadPart > maximum_bytes ?
            size.QuadPart - maximum_bytes : 0;
        success = SetFilePointerEx(source, offset, NULL, FILE_BEGIN);
    }
    while (success) {
        if (!ReadFile(source, buffer, sizeof(buffer), &read_count, NULL)) {
            success = FALSE;
            break;
        }
        if (read_count == 0u) break;
        if (!WriteFile(target, buffer, read_count, &written, NULL) ||
            written != read_count) success = FALSE;
    }
    CloseHandle(target);
    CloseHandle(source);
    return success;
}

static void view_runtime_log(HWND owner) {
    WCHAR game_directory[MAX_PATH];
    WCHAR log_path[MAX_PATH];
    WCHAR settings_directory[MAX_PATH];
    WCHAR recent_path[MAX_PATH];
    if (!selected_game_directory(game_directory, MAX_PATH) ||
        !join_path(log_path, MAX_PATH, game_directory, L"SudekiMP.log") ||
        !file_exists(log_path)) {
        show_error(owner, L"SudekiMP.log was not found beside the selected SUDEKI.exe.");
        return;
    }
    if (!get_settings_directory(settings_directory,
            sizeof(settings_directory) / sizeof(settings_directory[0])) ||
        !join_path(recent_path, MAX_PATH, settings_directory,
            L"SudekiMP-recent.log") ||
        !copy_file_tail(log_path, recent_path, 2u * 1024u * 1024u)) {
        show_error(owner, L"Windows could not prepare a bounded recent-log view.");
        return;
    }
    if ((INT_PTR)ShellExecuteW(owner, L"open", L"notepad.exe", recent_path,
            settings_directory, SW_SHOWNORMAL) <= 32) {
        show_error(owner, L"Windows could not open the runtime log.");
    }
}

static void export_support_logs(HWND owner) {
    BROWSEINFOW browse;
    PIDLIST_ABSOLUTE item;
    WCHAR destination[MAX_PATH];
    WCHAR game_directory[MAX_PATH];
    WCHAR bundle_directory[MAX_PATH];
    WCHAR source[MAX_PATH];
    WCHAR target[MAX_PATH];
    WCHAR summary[1024];
    SYSTEMTIME now;
    HANDLE file;
    DWORD written;
    if (!selected_game_directory(game_directory, MAX_PATH)) {
        show_error(owner, L"Choose the Sudeki folder before exporting logs.");
        return;
    }
    ZeroMemory(&browse, sizeof(browse));
    browse.hwndOwner = owner;
    browse.lpszTitle = L"Choose where to save the SudekiMP support folder";
    browse.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    item = SHBrowseForFolderW(&browse);
    if (item == NULL) return;
    if (!SHGetPathFromIDListW(item, destination)) {
        CoTaskMemFree(item);
        return;
    }
    CoTaskMemFree(item);
    GetLocalTime(&now);
    if (FAILED(StringCchPrintfW(bundle_directory, MAX_PATH,
            L"%s\\SudekiMP-support-%04u%02u%02u-%02u%02u%02u",
            destination, (unsigned int)now.wYear, (unsigned int)now.wMonth,
            (unsigned int)now.wDay, (unsigned int)now.wHour,
            (unsigned int)now.wMinute, (unsigned int)now.wSecond)) ||
        !create_directory_if_missing(bundle_directory)) {
        show_error(owner, L"Windows could not create the support folder.");
        return;
    }
    if (join_path(source, MAX_PATH, game_directory, L"SudekiMP.log") &&
        join_path(target, MAX_PATH, bundle_directory, L"SudekiMP.log") &&
        file_exists(source)) (void)copy_file_tail(
            source, target, 8u * 1024u * 1024u);
    if (join_path(source, MAX_PATH, package_directory, L"SudekiMP.ini") &&
        join_path(target, MAX_PATH, bundle_directory, L"SudekiMP.ini") &&
        file_exists(source)) (void)CopyFileW(source, target, FALSE);
    if (join_path(target, MAX_PATH, bundle_directory, L"launcher-summary.txt") &&
        SUCCEEDED(StringCchPrintfW(summary,
            sizeof(summary) / sizeof(summary[0]),
            L"launcher_version=%s\r\ngame_directory=%s\r\n"
            L"automatic_upload=false\r\n",
            SUDEKIMP_LAUNCHER_VERSION, game_directory))) {
        file = CreateFileW(target, GENERIC_WRITE, 0u, NULL, CREATE_ALWAYS,
            FILE_ATTRIBUTE_NORMAL, NULL);
        if (file != INVALID_HANDLE_VALUE) {
            WriteFile(file, summary,
                (DWORD)(lstrlenW(summary) * sizeof(WCHAR)), &written, NULL);
            CloseHandle(file);
        }
    }
    MessageBoxW(owner,
        L"The support folder was saved. Automatic log upload is intentionally "
        L"not enabled yet; share this folder manually when requested.",
        SUDEKIMP_TITLE, MB_OK | MB_ICONINFORMATION);
    ShellExecuteW(owner, L"open", bundle_directory, NULL, NULL, SW_SHOWNORMAL);
}

static void apply_default_font(HWND control) {
    SendMessageW(control,
                 WM_SETFONT,
                 (WPARAM)(body_font != NULL ? body_font : GetStockObject(DEFAULT_GUI_FONT)),
                 TRUE);
}

static void apply_font(HWND control, HFONT font) {
    SendMessageW(control,
                 WM_SETFONT,
                 (WPARAM)(font != NULL ? font : GetStockObject(DEFAULT_GUI_FONT)),
                 TRUE);
}

static COLORREF button_fill_color(unsigned int identifier, BOOL selected) {
    if (identifier == IDC_LAUNCH) {
        return selected ? RGB(20, 132, 160) : SUDEKIMP_COLOR_CYAN;
    }
    if (identifier == IDC_PLAY_MUSIC) {
        return selected ? RGB(42, 105, 144) : SUDEKIMP_COLOR_BLUE;
    }
    return selected ? RGB(53, 76, 101) : SUDEKIMP_COLOR_BUTTON;
}

static void draw_owner_button(const DRAWITEMSTRUCT *draw) {
    WCHAR text[128];
    RECT content = draw->rcItem;
    HBRUSH fill;
    HPEN outline;
    HGDIOBJ old_pen;
    HGDIOBJ old_brush;
    HGDIOBJ old_font;
    BOOL selected = (draw->itemState & ODS_SELECTED) != 0u;
    BOOL disabled = (draw->itemState & ODS_DISABLED) != 0u;
    COLORREF fill_color = disabled ? RGB(43, 52, 62) :
                                     button_fill_color(draw->CtlID, selected);

    /* RoundRect does not paint its corners. Clear the full owner-draw area
       first so the system button background cannot peek through there. */
    FillRect(draw->hDC, &content, app_background_brush);
    fill = CreateSolidBrush(fill_color);
    outline = CreatePen(PS_SOLID,
                        1,
                        (draw->CtlID == IDC_LAUNCH) ? RGB(145, 235, 245) :
                                                        RGB(77, 105, 133));
    old_brush = SelectObject(draw->hDC, fill);
    old_pen = SelectObject(draw->hDC, outline);
    RoundRect(draw->hDC,
              content.left,
              content.top,
              content.right,
              content.bottom,
              8,
              8);
    SelectObject(draw->hDC, old_brush);
    SelectObject(draw->hDC, old_pen);
    DeleteObject(fill);
    DeleteObject(outline);

    GetWindowTextW(draw->hwndItem, text, (int)(sizeof(text) / sizeof(text[0])));
    SetBkMode(draw->hDC, TRANSPARENT);
    SetTextColor(draw->hDC, disabled ? RGB(125, 140, 155) : SUDEKIMP_COLOR_TEXT);
    old_font = SelectObject(draw->hDC,
                            body_font != NULL ? body_font : GetStockObject(DEFAULT_GUI_FONT));
    DrawTextW(draw->hDC,
              text,
              -1,
              &content,
              DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    SelectObject(draw->hDC, old_font);
    if ((draw->itemState & ODS_FOCUS) != 0u) {
        InflateRect(&content, -3, -3);
        DrawFocusRect(draw->hDC, &content);
    }
}

static LRESULT CALLBACK launcher_window_proc(HWND window,
                                              UINT message,
                                              WPARAM wparam,
                                              LPARAM lparam) {
    switch (message) {
        case WM_DRAWITEM:
            if (lparam != 0) {
                const DRAWITEMSTRUCT *draw = (const DRAWITEMSTRUCT *)lparam;
                if (draw->CtlType == ODT_BUTTON) {
                    draw_owner_button(draw);
                    return TRUE;
                }
            }
            break;
        case WM_CTLCOLORSTATIC: {
            HDC control_dc = (HDC)wparam;
            const HWND control = (HWND)lparam;
            const int control_id = GetDlgCtrlID(control);
            SetTextColor(control_dc,
                         control_id == IDC_STATUS ? SUDEKIMP_COLOR_MUTED :
                         control_id == IDC_SAVE_WARNING ? RGB(244, 105, 105) :
                                                           SUDEKIMP_COLOR_TEXT);
            SetBkMode(control_dc, TRANSPARENT);
            if (GetDlgCtrlID(control) == IDC_STATUS) {
                return (LRESULT)panel_background_brush;
            }
            /* The window paints the shared background; returning an opaque
               brush here created the dark text-sized bars behind each label. */
            return (LRESULT)GetStockObject(HOLLOW_BRUSH);
        }
        case WM_CTLCOLOREDIT:
            SetTextColor((HDC)wparam, SUDEKIMP_COLOR_TEXT);
            SetBkColor((HDC)wparam, SUDEKIMP_COLOR_INPUT);
            return (LRESULT)input_background_brush;
        case WM_PAINT: {
            PAINTSTRUCT paint;
            HDC paint_dc = BeginPaint(window, &paint);
            RECT header = { 16, 14, 744, 102 };
            RECT status = { 24, 180, 704, 234 };
            HBRUSH header_brush = CreateSolidBrush(SUDEKIMP_COLOR_PANEL);
            FillRect(paint_dc, &header, header_brush);
            FillRect(paint_dc, &status, panel_background_brush);
            DeleteObject(header_brush);
            EndPaint(window, &paint);
            return 0;
        }
        case WM_COMMAND:
            switch (LOWORD(wparam)) {
                case IDC_BROWSE:
                    browse_for_game_directory(window);
                    return 0;
                case IDC_VERIFY:
                    {
                        WCHAR game_directory[MAX_PATH];
                        WCHAR loader_path[MAX_PATH];
                        WCHAR dll_path[MAX_PATH];
                        (void)verify_game(window, game_directory, loader_path, dll_path);
                    }
                    return 0;
                case IDC_LAUNCH:
                    launch_game(window);
                    return 0;
                case IDC_UPDATE:
                    check_for_launcher_update(window, FALSE);
                    return 0;
                case IDC_STOP_GAME:
                    stop_tracked_sudeki(window);
                    return 0;
                case IDC_VIEW_LOG:
                    view_runtime_log(window);
                    return 0;
                case IDC_EXPORT_LOGS:
                    export_support_logs(window);
                    return 0;
                case IDC_PROFILE:
                case IDC_LAN_HOST:
                case IDC_LAN_PORT:
                case IDC_AUTO_UPDATE:
                case IDC_CLEANROOM_TOOLS:
                    persist_launcher_options();
                    return 0;
                case IDC_INSTALL_COOP_SAVES:
                    (void)install_coop_save_fixtures(window);
                    return 0;
                case IDC_TEST_XINPUT:
                    test_xinput_controller(window);
                    return 0;
                case IDC_PLAY_MUSIC:
                    start_music_download(window);
                    return 0;
                case IDC_STOP_MUSIC:
                    close_music();
                    set_status(L"Project music stopped.");
                    return 0;
                case IDC_DEVELOPER:
                    ShellExecuteW(window,
                                  L"open",
                                  SUDEKIMP_PROJECT_URL,
                                  NULL,
                                  NULL,
                                  SW_SHOWNORMAL);
                    return 0;
                default:
                    break;
            }
            break;
        case WM_SUDEKIMP_MUSIC_COMPLETE:
            if (wparam != 0u) {
                (void)play_cached_music(window);
            } else {
                show_error(window,
                           L"The project music catalog or track could not be downloaded. "
                           L"The launcher never changed the game installation.");
                set_status(L"Project music is unavailable. Check your connection and try again.");
            }
            return 0;
        case WM_DESTROY:
            persist_launcher_options();
            close_music();
            if (launched_game_job != NULL) {
                CloseHandle(launched_game_job);
                launched_game_job = NULL;
            }
            DeleteObject(app_background_brush);
            DeleteObject(panel_background_brush);
            DeleteObject(input_background_brush);
            DeleteObject(body_font);
            DeleteObject(title_font);
            DeleteObject(subtitle_font);
            PostQuitMessage(0);
            return 0;
        default:
            break;
    }
    return DefWindowProcW(window, message, wparam, lparam);
}

int WINAPI wWinMain(HINSTANCE instance,
                    HINSTANCE previous_instance,
                    PWSTR command_line,
                    int show_command) {
    WNDCLASSEXW window_class;
    HWND control;
    HICON icon;
    HICON displayed_icon;
    MSG message;
    (void)previous_instance;
    (void)command_line;

    launcher_instance = instance;
    if (!initialise_package_directory()) {
        MessageBoxW(NULL,
                    L"SudekiMP could not determine its own package folder.",
                    SUDEKIMP_TITLE,
                    MB_OK | MB_ICONERROR);
        return 1;
    }
    app_background_brush = CreateSolidBrush(SUDEKIMP_COLOR_BACKGROUND);
    panel_background_brush = CreateSolidBrush(SUDEKIMP_COLOR_PANEL);
    input_background_brush = CreateSolidBrush(SUDEKIMP_COLOR_INPUT);
    body_font = CreateFontW(-15,
                            0,
                            0,
                            0,
                            FW_SEMIBOLD,
                            FALSE,
                            FALSE,
                            FALSE,
                            DEFAULT_CHARSET,
                            OUT_DEFAULT_PRECIS,
                            CLIP_DEFAULT_PRECIS,
                            CLEARTYPE_QUALITY,
                            DEFAULT_PITCH | FF_DONTCARE,
                            L"Segoe UI");
    title_font = CreateFontW(-23,
                             0,
                             0,
                             0,
                             FW_BOLD,
                             FALSE,
                             FALSE,
                             FALSE,
                             DEFAULT_CHARSET,
                             OUT_DEFAULT_PRECIS,
                             CLIP_DEFAULT_PRECIS,
                             CLEARTYPE_QUALITY,
                             DEFAULT_PITCH | FF_DONTCARE,
                             L"Segoe UI");
    subtitle_font = CreateFontW(-14,
                                0,
                                0,
                                0,
                                FW_NORMAL,
                                FALSE,
                                FALSE,
                                FALSE,
                                DEFAULT_CHARSET,
                                OUT_DEFAULT_PRECIS,
                                CLIP_DEFAULT_PRECIS,
                                CLEARTYPE_QUALITY,
                                DEFAULT_PITCH | FF_DONTCARE,
                                L"Segoe UI");
    ZeroMemory(&window_class, sizeof(window_class));
    window_class.cbSize = sizeof(window_class);
    window_class.hInstance = instance;
    window_class.hCursor = LoadCursorW(NULL, IDC_ARROW);
    window_class.hbrBackground = app_background_brush;
    window_class.lpfnWndProc = launcher_window_proc;
    window_class.lpszClassName = L"SudekiMPBetaLauncher";
    window_class.hIcon = LoadIconW(instance, MAKEINTRESOURCEW(IDI_SUDEKIMP_BETA));
    window_class.hIconSm = window_class.hIcon;
    if (RegisterClassExW(&window_class) == 0u) {
        return 1;
    }

    launcher_window = CreateWindowExW(0u,
                                       window_class.lpszClassName,
                                       SUDEKIMP_TITLE,
                                       WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU |
                                           WS_MINIMIZEBOX,
                                       CW_USEDEFAULT,
                                       CW_USEDEFAULT,
                                       760,
                                       668,
                                       NULL,
                                       NULL,
                                       instance,
                                       NULL);
    if (launcher_window == NULL) {
        return 1;
    }
    icon = LoadIconW(instance, MAKEINTRESOURCEW(IDI_SUDEKIMP_BETA));
    displayed_icon = (HICON)LoadImageW(instance,
                                       MAKEINTRESOURCEW(IDI_SUDEKIMP_BETA),
                                       IMAGE_ICON,
                                       64,
                                       64,
                                       LR_DEFAULTCOLOR);
    SendMessageW(launcher_window, WM_SETICON, ICON_BIG, (LPARAM)icon);
    SendMessageW(launcher_window, WM_SETICON, ICON_SMALL, (LPARAM)icon);

    control = CreateWindowW(L"STATIC",
                            L"",
                            WS_CHILD | WS_VISIBLE | SS_ICON,
                            30,
                            26,
                            64,
                            64,
                            launcher_window,
                            (HMENU)(INT_PTR)IDC_PROJECT_ICON,
                            instance,
                            NULL);
    SendMessageW(control,
                 STM_SETICON,
                 (WPARAM)(displayed_icon != NULL ? displayed_icon : icon),
                 0);
    control = CreateWindowW(L"STATIC",
                            L"SUDEKI TOGETHER",
                            WS_CHILD | WS_VISIBLE,
                            116,
                            27,
                            530,
                            28,
                            launcher_window,
                            NULL,
                            instance,
                            NULL);
    apply_font(control, title_font);
    control = CreateWindowW(L"STATIC",
                            L"Local co-op • LAN arena • cleanroom • exact-build validation",
                            WS_CHILD | WS_VISIBLE,
                            117,
                            61,
                            570,
                            24,
                            launcher_window,
                            NULL,
                            instance,
                            NULL);
    apply_font(control, subtitle_font);
    control = CreateWindowW(L"STATIC",
                            L"Sudeki folder (paste the folder that contains SUDEKI.exe):",
                            WS_CHILD | WS_VISIBLE,
                            28,
                            119,
                            620,
                            20,
                            launcher_window,
                            NULL,
                            instance,
                            NULL);
    apply_default_font(control);
    directory_edit = CreateWindowExW(WS_EX_CLIENTEDGE,
                                     L"EDIT",
                                     L"",
                                     WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
                                     28,
                                     143,
                                     548,
                                     30,
                                     launcher_window,
                                     (HMENU)(INT_PTR)IDC_GAME_DIRECTORY,
                                     instance,
                                     NULL);
    apply_default_font(directory_edit);
    control = CreateWindowW(L"BUTTON",
                            L"Browse…",
                            WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
                            590,
                            142,
                            132,
                            32,
                            launcher_window,
                            (HMENU)(INT_PTR)IDC_BROWSE,
                            instance,
                            NULL);
    apply_default_font(control);
    status_label = CreateWindowW(L"STATIC",
                                 L"Paste or choose the supported GOG Sudeki folder, then verify it.",
                                 WS_CHILD | WS_VISIBLE,
                                 40,
                                 196,
                                 650,
                                 24,
                                 launcher_window,
                                 (HMENU)(INT_PTR)IDC_STATUS,
                                 instance,
                                 NULL);
    apply_default_font(status_label);
    control = CreateWindowW(L"STATIC", L"Profile:", WS_CHILD | WS_VISIBLE,
                            28, 222, 72, 22, launcher_window, NULL, instance, NULL);
    apply_default_font(control);
    profile_combo = CreateWindowW(L"COMBOBOX", L"",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST,
        100, 218, 315, 160, launcher_window,
        (HMENU)(INT_PTR)IDC_PROFILE, instance, NULL);
    apply_default_font(profile_combo);
    SendMessageW(profile_combo, CB_ADDSTRING, 0, (LPARAM)L"Local co-op (2 players)");
    SendMessageW(profile_combo, CB_ADDSTRING, 0, (LPARAM)L"LAN arena host — Tal");
    SendMessageW(profile_combo, CB_ADDSTRING, 0, (LPARAM)L"LAN arena client — Ailish");
    SendMessageW(profile_combo, CB_ADDSTRING, 0, (LPARAM)L"Cleanroom");
    SendMessageW(profile_combo, CB_ADDSTRING, 0, (LPARAM)L"Safe launch");
    SendMessageW(profile_combo, CB_SETCURSEL, SUDEKIMP_PROFILE_LOCAL_COOP, 0);
    control = CreateWindowW(L"STATIC", L"LAN IP:", WS_CHILD | WS_VISIBLE,
                            430, 222, 60, 22, launcher_window, NULL, instance, NULL);
    apply_default_font(control);
    lan_host_edit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"127.0.0.1",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
        488, 218, 142, 28, launcher_window,
        (HMENU)(INT_PTR)IDC_LAN_HOST, instance, NULL);
    apply_default_font(lan_host_edit);
    lan_port_edit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"26770",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_NUMBER,
        640, 218, 82, 28, launcher_window,
        (HMENU)(INT_PTR)IDC_LAN_PORT, instance, NULL);
    apply_default_font(lan_port_edit);
    auto_update_checkbox = CreateWindowW(L"BUTTON",
        L"Check for updates on startup (prompt before download)",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX,
        28, 254, 430, 22, launcher_window,
        (HMENU)(INT_PTR)IDC_AUTO_UPDATE, instance, NULL);
    apply_default_font(auto_update_checkbox);
    cleanroom_tools_checkbox = CreateWindowW(L"BUTTON",
        L"Enable cleanroom sandbox tools (F8): actors, dummy, combat/camera, inventory, infinite meters",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX,
        28, 278, 690, 22, launcher_window,
        (HMENU)(INT_PTR)IDC_CLEANROOM_TOOLS, instance, NULL);
    apply_default_font(cleanroom_tools_checkbox);
    SendMessageW(cleanroom_tools_checkbox, BM_SETCHECK, BST_CHECKED, 0);
    control = CreateWindowW(L"BUTTON",
                            L"Verify build",
                            WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
                            28,
                            314,
                            150,
                            38,
                            launcher_window,
                            (HMENU)(INT_PTR)IDC_VERIFY,
                            instance,
                            NULL);
    apply_default_font(control);
    control = CreateWindowW(L"BUTTON",
                            L"Launch SudekiMP",
                            WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
                            190,
                            314,
                            190,
                            38,
                            launcher_window,
                            (HMENU)(INT_PTR)IDC_LAUNCH,
                            instance,
                            NULL);
    apply_default_font(control);
    control = CreateWindowW(L"BUTTON",
                            L"Stop tracked game",
                            WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
                            392,
                            314,
                            170,
                            38,
                            launcher_window,
                            (HMENU)(INT_PTR)IDC_STOP_GAME,
                            instance,
                            NULL);
    apply_default_font(control);
    control = CreateWindowW(L"BUTTON",
                            L"View runtime log",
                            WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
                            28,
                            366,
                            200,
                            34,
                            launcher_window,
                            (HMENU)(INT_PTR)IDC_VIEW_LOG,
                            instance,
                            NULL);
    apply_default_font(control);
    control = CreateWindowW(L"BUTTON",
                            L"Export support logs…",
                            WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
                            240,
                            366,
                            220,
                            34,
                            launcher_window,
                            (HMENU)(INT_PTR)IDC_EXPORT_LOGS,
                            instance,
                            NULL);
    apply_default_font(control);
    control = CreateWindowW(L"BUTTON", L"Check for updates…",
                            WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
                            472, 366, 190, 34, launcher_window,
                            (HMENU)(INT_PTR)IDC_UPDATE, instance, NULL);
    apply_default_font(control);
    control = CreateWindowW(L"BUTTON", L"Install co-op save fixtures…",
                            WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
                            28, 416, 230, 34, launcher_window,
                            (HMENU)(INT_PTR)IDC_INSTALL_COOP_SAVES, instance, NULL);
    apply_default_font(control);
    control = CreateWindowW(L"BUTTON", L"Test XInput controller…",
                            WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
                            270, 416, 210, 34, launcher_window,
                            (HMENU)(INT_PTR)IDC_TEST_XINPUT, instance, NULL);
    apply_default_font(control);
    control = CreateWindowW(L"BUTTON",
                            L"Play project music",
                            WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
                            28,
                            466,
                            190,
                            34,
                            launcher_window,
                            (HMENU)(INT_PTR)IDC_PLAY_MUSIC,
                            instance,
                            NULL);
    apply_default_font(control);
    control = CreateWindowW(L"BUTTON",
                            L"Stop music",
                            WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
                            230,
                            466,
                            130,
                            34,
                            launcher_window,
                            (HMENU)(INT_PTR)IDC_STOP_MUSIC,
                            instance,
                            NULL);
    apply_default_font(control);
    control = CreateWindowW(L"BUTTON",
                            L"Developer: wander",
                            WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
                            530,
                            466,
                            192,
                            34,
                            launcher_window,
                            (HMENU)(INT_PTR)IDC_DEVELOPER,
                            instance,
                            NULL);
    apply_default_font(control);
    control = CreateWindowW(L"STATIC",
                            L"LAN arena and cleanroom do not read campaign saves. Talos research flags are not exposed. "
                            L"Support logs are exported locally for manual sharing; nothing is uploaded automatically.",
                            WS_CHILD | WS_VISIBLE | SS_LEFT,
                            28, 520, 680, 48, launcher_window,
                            (HMENU)(INT_PTR)IDC_SAVE_WARNING, instance, NULL);
    apply_default_font(control);

    load_saved_game_directory();
    ShowWindow(launcher_window, show_command);
    UpdateWindow(launcher_window);
    if (auto_update_checkbox != NULL &&
        SendMessageW(auto_update_checkbox, BM_GETCHECK, 0, 0) == BST_CHECKED) {
        check_for_launcher_update(launcher_window, TRUE);
    }
    while (GetMessageW(&message, NULL, 0u, 0u) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    return (int)message.wParam;
}
