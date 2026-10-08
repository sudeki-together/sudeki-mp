#define _WIN32_WINNT 0x0600

#include <windows.h>
#include <commctrl.h>
#include <mmsystem.h>
#include <shellapi.h>
#include <shlobj.h>
#include <strsafe.h>
#include <tlhelp32.h>
#include <urlmon.h>

#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#include "beta_launcher_resource.h"
#include "mp3_wave.h"
#include "options_store.h"
#include "player_options.h"
#include "settings_page.h"
#include "mods_panel.h"

#define SUDEKIMP_TITLE L"SudekiMP Launcher"
#define SUDEKIMP_LAUNCHER_VERSION L"0.5.0"
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
/* Cleanroom only: the hero the test room starts as (combo order below). */
static HWND cleanroom_lead_label;
/* Developer mode (Tools): shows the LAN arena test profiles and their address row. */
static BOOL developer_mode;
static HWND developer_mode_checkbox;
static HWND lan_row[4];
static HWND cleanroom_lead_combo;
/* Title-menu multiplayer only: the lobby's [TitleMenu] Scope (combo order). */
static HWND title_mode_label;
static HWND title_mode_combo;
static const WCHAR *const title_mode_names[] = {L"Test room", L"Saved story"};
static const WCHAR *const title_mode_scopes[] = {L"entry", L"saved-story"};
static const WCHAR *const cleanroom_lead_names[] = {L"Ailish", L"Tal", L"Elco", L"Buki"};
static const WCHAR *const cleanroom_lead_arguments[] = {
    L" --game-arg=-Ailish --game-arg=1", L" --game-arg=-Tal --game-arg=1",
    L" --game-arg=-Elco --game-arg=1", L" --game-arg=-Buki --game-arg=1"};
static WCHAR package_directory[MAX_PATH];
static WCHAR music_cache_path[MAX_PATH];
/* Mini player (bottom of the left column, every tab). */
typedef enum SudekiMpMusicState {
    SUDEKIMP_MUSIC_STOPPED = 0, SUDEKIMP_MUSIC_LOADING, SUDEKIMP_MUSIC_PLAYING,
    SUDEKIMP_MUSIC_PAUSED
} SudekiMpMusicState;
static SudekiMpMusicState music_state;
static HWND music_title_label, music_state_label, music_play_button, music_stop_button;
#define SUDEKIMP_MUSIC_X (SUDEKIMP_ART_X)
#define SUDEKIMP_MUSIC_Y 552
#define SUDEKIMP_MUSIC_TIMER 7u
static void close_music(void);
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
    SUDEKIMP_PROFILE_SAFE = 4,
    SUDEKIMP_PROFILE_TITLE_MULTIPLAYER = 5,
    SUDEKIMP_PROFILE_LAST = SUDEKIMP_PROFILE_TITLE_MULTIPLAYER
} SudekiMpLauncherProfile;

#define SUDEKIMP_COLOR_BACKGROUND RGB(12, 20, 31)
#define SUDEKIMP_COLOR_PANEL RGB(23, 34, 49)
#define SUDEKIMP_COLOR_INPUT RGB(17, 27, 40)
#define SUDEKIMP_COLOR_TEXT RGB(232, 240, 248)
#define SUDEKIMP_COLOR_MUTED RGB(159, 181, 202)
#define SUDEKIMP_COLOR_CYAN RGB(54, 193, 218)
#define SUDEKIMP_COLOR_BLUE RGB(40, 100, 168)
#define SUDEKIMP_COLOR_BUTTON RGB(38, 55, 75)

/* Layout mirrors the vanilla Sudeki launcher: 220-pixel art column on the
   left (logo 220x101, portrait 220x300), tabbed page on the right. */
#define SUDEKIMP_CLIENT_W 896
#define SUDEKIMP_CLIENT_H 644
#define SUDEKIMP_ART_X 16
#define SUDEKIMP_ART_Y 16
#define SUDEKIMP_ART_W 220
#define SUDEKIMP_LOGO_H 101
#define SUDEKIMP_PORTRAIT_Y 129
#define SUDEKIMP_PORTRAIT_H 300
#define SUDEKIMP_IDENTITY_Y 446
#define SUDEKIMP_PAGE_X 252
#define SUDEKIMP_PAGE_Y 52
#define SUDEKIMP_PAGE_W 628
#define SUDEKIMP_PAGE_H 488
#define SUDEKIMP_CONTENT_X (SUDEKIMP_PAGE_X + 16)
#define SUDEKIMP_CONTENT_Y (SUDEKIMP_PAGE_Y + 16)
#define SUDEKIMP_CONTENT_W (SUDEKIMP_PAGE_W - 32)
#define SUDEKIMP_STATUS_Y 552
#define SUDEKIMP_ACTIONS_Y 594

typedef enum SudekiMpLauncherTab {
    SUDEKIMP_TAB_PLAY = 0,
    SUDEKIMP_TAB_VIDEO_AUDIO,
    SUDEKIMP_TAB_CONTROLS,
    SUDEKIMP_TAB_CONTROLLER,
    SUDEKIMP_TAB_MODS,
    SUDEKIMP_TAB_TOOLS,
    SUDEKIMP_TAB_COUNT
} SudekiMpLauncherTab;

/* Controls registered with this owner stay visible on every tab. */
#define SUDEKIMP_TAB_ALWAYS (-1)
/* The left art column: every tab except Mods, whose workshop uses the full width. */
#define SUDEKIMP_TAB_SIDEBAR (-2)
/* The Mods workshop: left edge of the art column to the right edge of the page. */
#define SUDEKIMP_MODS_X SUDEKIMP_ART_X
#define SUDEKIMP_MODS_W (SUDEKIMP_PAGE_X + SUDEKIMP_PAGE_W - SUDEKIMP_ART_X)
#define SUDEKIMP_MAX_CONTROLS 128u

typedef enum SudekiMpLauncherArt {
    SUDEKIMP_ART_LOGO = 0,
    SUDEKIMP_ART_TAL,
    SUDEKIMP_ART_AILISH,
    SUDEKIMP_ART_BUKI,
    SUDEKIMP_ART_ELCO,
    SUDEKIMP_ART_CONTROLLER_MAP,
    SUDEKIMP_ART_COUNT
} SudekiMpLauncherArt;

typedef struct SudekiMpLauncherControl {
    HWND window;
    int tab;
} SudekiMpLauncherControl;

static const int tab_art[SUDEKIMP_TAB_COUNT] = {
    SUDEKIMP_ART_AILISH, SUDEKIMP_ART_TAL, SUDEKIMP_ART_BUKI,
    SUDEKIMP_ART_ELCO, SUDEKIMP_ART_AILISH, SUDEKIMP_ART_ELCO
};
/* Values accepted by the DLL's StoryTestBoostMultiplier range (1.0-4.0). */
static const WCHAR *const story_boost_multipliers[] = {L"1.5", L"2.0", L"3.0", L"4.0"};

static SudekiMpLauncherControl launcher_controls[SUDEKIMP_MAX_CONTROLS];
static size_t launcher_control_count;
static int active_tab = SUDEKIMP_TAB_PLAY;
static HWND mods_panel;
static HBITMAP launcher_art[SUDEKIMP_ART_COUNT];
static SudekiMpSettingsControls settings_controls;
static SudekiMpPlayerOptions player_options;
static SudekiMpOptionsSource player_options_source;
static HWND options_source_label;
static HWND skip_movies_checkbox;
static HWND quick_menu_speed_checkbox;
static HWND story_boost_checkbox;
static HWND story_boost_combo;
static BOOL settings_dirty;
static BOOL populating_settings;

static void on_game_directory_changed(void);
static BOOL apply_mod_options(const WCHAR *config_path, SudekiMpLauncherProfile profile);
static BOOL save_all_settings(HWND owner);

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

static const struct { int profile; const WCHAR *label; BOOL developer; } profile_entries[] = {
    {SUDEKIMP_PROFILE_LOCAL_COOP, L"Local co-op (2 players)", FALSE},
    {SUDEKIMP_PROFILE_TITLE_MULTIPLAYER, L"Multiplayer (title menu lobby)", FALSE},
    {SUDEKIMP_PROFILE_LAN_HOST, L"LAN arena host — Tal (developer)", TRUE},
    {SUDEKIMP_PROFILE_LAN_CLIENT, L"LAN arena client — Ailish (developer)", TRUE},
    {SUDEKIMP_PROFILE_CLEANROOM, L"Cleanroom (test room)", FALSE},
    {SUDEKIMP_PROFILE_SAFE, L"Safe launch", FALSE}
};

/* The list holds only the profiles available in the current mode; each
   entry carries its profile number, so list positions never matter. */
static int current_profile(void) {
    LRESULT index = profile_combo == NULL ? CB_ERR :
        SendMessageW(profile_combo, CB_GETCURSEL, 0, 0);
    LRESULT data = index == CB_ERR ? CB_ERR :
        SendMessageW(profile_combo, CB_GETITEMDATA, (WPARAM)index, 0);
    return data >= SUDEKIMP_PROFILE_LOCAL_COOP && data <= SUDEKIMP_PROFILE_LAST ?
        (int)data : SUDEKIMP_PROFILE_LOCAL_COOP;
}

static void set_profile(int profile) {
    LRESULT count, index;
    if (profile_combo == NULL) return;
    count = SendMessageW(profile_combo, CB_GETCOUNT, 0, 0);
    for (index = 0; index < count; ++index) {
        if (SendMessageW(profile_combo, CB_GETITEMDATA, (WPARAM)index, 0) == profile) {
            SendMessageW(profile_combo, CB_SETCURSEL, (WPARAM)index, 0);
            return;
        }
    }
    SendMessageW(profile_combo, CB_SETCURSEL, 0, 0); /* hidden profile: Local co-op */
}

static void rebuild_profile_list(void) {
    const int keep = current_profile();
    size_t index;
    if (profile_combo == NULL) return;
    SendMessageW(profile_combo, CB_RESETCONTENT, 0, 0);
    for (index = 0u; index < sizeof(profile_entries) / sizeof(profile_entries[0]); ++index) {
        LRESULT item;
        if (profile_entries[index].developer && !developer_mode) continue;
        item = SendMessageW(profile_combo, CB_ADDSTRING, 0, (LPARAM)profile_entries[index].label);
        SendMessageW(profile_combo, CB_SETITEMDATA, (WPARAM)item,
                     (LPARAM)profile_entries[index].profile);
    }
    set_profile(keep);
}

static int selected_cleanroom_lead(void) {
    int lead = cleanroom_lead_combo == NULL ? 0 :
        (int)SendMessageW(cleanroom_lead_combo, CB_GETCURSEL, 0, 0);
    return lead >= 0 && lead < (int)(sizeof(cleanroom_lead_names) / sizeof(cleanroom_lead_names[0])) ?
        lead : 0;
}

static int selected_title_mode(void) {
    int mode = title_mode_combo == NULL ? 0 :
        (int)SendMessageW(title_mode_combo, CB_GETCURSEL, 0, 0);
    return mode >= 0 && mode < (int)(sizeof(title_mode_names) / sizeof(title_mode_names[0])) ?
        mode : 0;
}

static void persist_launcher_options(void) {
    WCHAR settings_path[MAX_PATH];
    WCHAR value[64];
    int profile;
    if (!get_settings_path(settings_path,
            sizeof(settings_path) / sizeof(settings_path[0]))) return;
    profile = current_profile();
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
    WritePrivateProfileStringW(L"launcher", L"developer_mode",
        developer_mode ? L"true" : L"false", settings_path);
    WritePrivateProfileStringW(L"launcher", L"cleanroom_lead",
        cleanroom_lead_names[selected_cleanroom_lead()], settings_path);
    WritePrivateProfileStringW(L"launcher", L"title_mode",
        title_mode_scopes[selected_title_mode()], settings_path);
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
    {
        WCHAR flag[16];
        GetPrivateProfileStringW(L"launcher", L"developer_mode", L"false", flag, 16,
                                 settings_path);
        developer_mode = lstrcmpiW(flag, L"true") == 0;
        if (developer_mode_checkbox != NULL) {
            SendMessageW(developer_mode_checkbox, BM_SETCHECK,
                         developer_mode ? BST_CHECKED : BST_UNCHECKED, 0);
        }
        rebuild_profile_list();
    }
    if (profile_combo != NULL) {
        int profile = GetPrivateProfileIntW(L"launcher", L"profile",
            SUDEKIMP_PROFILE_LOCAL_COOP, settings_path);
        if (profile < SUDEKIMP_PROFILE_LOCAL_COOP || profile > SUDEKIMP_PROFILE_LAST) {
            profile = SUDEKIMP_PROFILE_LOCAL_COOP;
        }
        set_profile(profile);
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
    if (cleanroom_lead_combo != NULL) {
        size_t index;
        GetPrivateProfileStringW(L"launcher", L"cleanroom_lead", L"Ailish",
            saved_directory, (DWORD)(sizeof(saved_directory) /
                sizeof(saved_directory[0])), settings_path);
        SendMessageW(cleanroom_lead_combo, CB_SETCURSEL, 0, 0);
        for (index = 0u; index < sizeof(cleanroom_lead_names) / sizeof(cleanroom_lead_names[0]);
             ++index) {
            if (lstrcmpiW(saved_directory, cleanroom_lead_names[index]) == 0) {
                SendMessageW(cleanroom_lead_combo, CB_SETCURSEL, index, 0);
            }
        }
    }
    if (title_mode_combo != NULL) {
        size_t index;
        GetPrivateProfileStringW(L"launcher", L"title_mode", L"entry",
            saved_directory, (DWORD)(sizeof(saved_directory) /
                sizeof(saved_directory[0])), settings_path);
        SendMessageW(title_mode_combo, CB_SETCURSEL, 0, 0);
        for (index = 0u; index < sizeof(title_mode_scopes) / sizeof(title_mode_scopes[0]);
             ++index) {
            if (lstrcmpiW(saved_directory, title_mode_scopes[index]) == 0) {
                SendMessageW(title_mode_combo, CB_SETCURSEL, index, 0);
            }
        }
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
    const WCHAR *lead_argument = L"";
    if (!join_path(game_executable, MAX_PATH, game_directory, L"SUDEKI.exe")) {
        return FALSE;
    }
    if (!check_only) {
        if (profile == SUDEKIMP_PROFILE_LAN_HOST) {
            game_arguments = L" --game-arg=-Level --game-arg=testroom "
                L"--game-arg=-DT --game-arg=1 --game-arg=-Tal --game-arg=1";
        } else if (profile == SUDEKIMP_PROFILE_LAN_CLIENT) {
            game_arguments = L" --game-arg=-Level --game-arg=testroom "
                L"--game-arg=-DT --game-arg=1 --game-arg=-Ailish --game-arg=1";
        } else if (profile == SUDEKIMP_PROFILE_CLEANROOM) {
            /* The test room starts as the hero chosen in "Start as". */
            game_arguments = L" --game-arg=-Level --game-arg=testroom --game-arg=-DT --game-arg=1";
            lead_argument = cleanroom_lead_arguments[selected_cleanroom_lead()];
        }
    }
    return SUCCEEDED(StringCchPrintfW(command,
                                      command_count,
                                      check_only
                                          ? L"\"%s\" --check \"%s\" \"%s\""
                                          : L"\"%s\" \"%s\" \"%s\"%s%s",
                                      loader_path,
                                      game_executable,
                                      dll_path,
                                      game_arguments,
                                      lead_argument));
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
    /* [TitleMenu] lives outside [SudekiMP]: every other profile turns it off. */
    if (!disable_all_optional_profiles(config_path) ||
        !WritePrivateProfileStringW(L"TitleMenu", L"Enabled", L"false", config_path)) {
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
    } else if (profile == SUDEKIMP_PROFILE_TITLE_MULTIPLAYER) {
        /* The title screen gains a Multiplayer menu; its lobby starts each
           player's game itself (through the loader), in the chosen mode. */
        if (!WritePrivateProfileStringW(L"SudekiMP", L"SkipStartupMovies", L"true",
                                         config_path) ||
            !WritePrivateProfileStringW(L"TitleMenu", L"Enabled", L"true", config_path) ||
            !WritePrivateProfileStringW(L"TitleMenu", L"Scope",
                title_mode_scopes[selected_title_mode()], config_path)) {
            show_error(owner, L"SudekiMP could not write the title-menu multiplayer profile.");
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
    if (!apply_mod_options(config_path, profile)) {
        show_error(owner, L"SudekiMP could not write the SudekiMP options (Tools tab).");
        return FALSE;
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
    int selected_profile = current_profile();
    SudekiMpLauncherProfile profile;

    if (selected_profile < SUDEKIMP_PROFILE_LOCAL_COOP ||
        selected_profile > SUDEKIMP_PROFILE_LAST) {
        selected_profile = SUDEKIMP_PROFILE_LOCAL_COOP;
    }
    profile = (SudekiMpLauncherProfile)selected_profile;

    if (!save_all_settings(owner) ||
        !configure_launcher_profile(owner, profile) ||
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
    /* The game is starting: the launcher's music stops (Play resumes it later). */
    if (music_state != SUDEKIMP_MUSIC_STOPPED) {
        close_music();
    }
    if (profile == SUDEKIMP_PROFILE_LAN_HOST) {
        set_status(L"LAN arena host started as Tal. Keep the loader console for errors.");
    } else if (profile == SUDEKIMP_PROFILE_LAN_CLIENT) {
        set_status(L"LAN arena client started as Ailish. Keep the loader console for errors.");
    } else if (profile == SUDEKIMP_PROFILE_CLEANROOM) {
        WCHAR text[160];
        StringCchPrintfW(text, sizeof(text) / sizeof(text[0]),
            cleanroom_tools_checkbox != NULL &&
                SendMessageW(cleanroom_tools_checkbox, BM_GETCHECK, 0, 0) == BST_CHECKED ?
            L"Test room started as %ls. Press F8 for sandbox tools; campaign saves are not used." :
            L"Test room started as %ls with F8 tools disabled; campaign saves are not used.",
            cleanroom_lead_names[selected_cleanroom_lead()]);
        set_status(text);
    } else if (profile == SUDEKIMP_PROFILE_TITLE_MULTIPLAYER) {
        set_status(selected_title_mode() == 0 ?
            L"Started with the title-screen Multiplayer menu (test room lobby)." :
            L"Started with the title-screen Multiplayer menu (saved story lobby).");
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
            on_game_directory_changed();
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

static void update_music_player(void);
static void start_music_download(HWND owner);

static void close_music(void) {
    mciSendStringW(L"close SudekiMPMusic", NULL, 0u, NULL);
    music_state = SUDEKIMP_MUSIC_STOPPED;
    update_music_player();
}

/* Writes "<track>.wav" beside the cached MP3: the same MP3 frames in a RIFF
   container that the waveaudio device decodes through the ACM codec. */
static BOOL write_wave_wrapped_music(WCHAR wave_path[MAX_PATH]) {
    HANDLE file;
    LARGE_INTEGER size;
    DWORD transferred = 0u;
    unsigned char *mp3;
    unsigned char *wave = NULL;
    size_t wave_count = 0u;
    WCHAR *extension;
    BOOL ok = FALSE;

    if (FAILED(StringCchCopyW(wave_path, MAX_PATH, music_cache_path)) ||
        (extension = wcsrchr(wave_path, L'.')) == NULL ||
        FAILED(StringCchCopyW(extension, (size_t)(wave_path + MAX_PATH - extension),
                              L".wav"))) {
        return FALSE;
    }
    file = CreateFileW(music_cache_path, GENERIC_READ, FILE_SHARE_READ, NULL,
                       OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        return FALSE;
    }
    if (!GetFileSizeEx(file, &size) || size.QuadPart <= 0 ||
        size.QuadPart > 64 * 1024 * 1024 ||
        (mp3 = (unsigned char *)malloc((size_t)size.QuadPart)) == NULL) {
        CloseHandle(file);
        return FALSE;
    }
    if (ReadFile(file, mp3, (DWORD)size.QuadPart, &transferred, NULL) &&
        transferred == (DWORD)size.QuadPart) {
        ok = SudekiMpWrapMp3InWave(mp3, transferred, &wave, &wave_count);
    }
    CloseHandle(file);
    free(mp3);
    if (!ok) {
        return FALSE;
    }
    file = CreateFileW(wave_path, GENERIC_WRITE, 0u, NULL, CREATE_ALWAYS,
                       FILE_ATTRIBUTE_NORMAL, NULL);
    ok = file != INVALID_HANDLE_VALUE &&
         WriteFile(file, wave, (DWORD)wave_count, &transferred, NULL) &&
         transferred == wave_count;
    if (file != INVALID_HANDLE_VALUE) {
        CloseHandle(file);
    }
    free(wave);
    return ok;
}

static BOOL play_cached_music(HWND owner) {
    WCHAR command[MAX_PATH + 80u];
    WCHAR wave_path[MAX_PATH];
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
    /* Wine's mpegvideo (DirectShow) device fails to open the track with MCI
       error 277; its waveaudio device plays the same frames via ACM. */
    if (result != 0u && write_wave_wrapped_music(wave_path) &&
        SUCCEEDED(StringCchPrintfW(command,
                                   sizeof(command) / sizeof(command[0]),
                                   L"open \"%s\" type waveaudio alias SudekiMPMusic",
                                   wave_path))) {
        result = mciSendStringW(command, NULL, 0u, NULL);
    }
    if (result == 0u) {
        (void)mciSendStringW(L"set SudekiMPMusic time format milliseconds", NULL, 0u, NULL);
        /* notify: MM_MCINOTIFY tells the mini player when the track ends. */
        result = mciSendStringW(L"play SudekiMPMusic from 0 notify", NULL, 0u, launcher_window);
    }
    if (result != 0u) {
        close_music();
        show_error(owner,
                   L"Windows could not play the cached MP3. The launcher itself is unaffected.");
        return FALSE;
    }
    music_state = SUDEKIMP_MUSIC_PLAYING;
    update_music_player();
    set_status(L"Playing Map Inversion from the project music cache.");
    return TRUE;
}

/* Track name for the mini player: the cached file's name without ".mp3". */
static void music_title(WCHAR *out, size_t count) {
    const WCHAR *name = wcsrchr(music_cache_path, L'\\');
    WCHAR *dot;
    StringCchCopyW(out, count, name != NULL ? name + 1 : L"Map Inversion.mp3");
    if (out[0] == L'\0') StringCchCopyW(out, count, L"Map Inversion.mp3");
    dot = wcsrchr(out, L'.');
    if (dot != NULL) *dot = L'\0';
}

static void update_music_player(void) {
    static const WCHAR *const states[] = {L"Stopped", L"Loading…", L"Playing", L"Paused"};
    WCHAR title[MAX_PATH];
    if (music_title_label == NULL) return;
    music_title(title, MAX_PATH);
    SetWindowTextW(music_title_label, title);
    SetWindowTextW(music_state_label, states[music_state]);
    EnableWindow(music_stop_button, music_state != SUDEKIMP_MUSIC_STOPPED);
    InvalidateRect(music_play_button, NULL, FALSE);
    if (music_state == SUDEKIMP_MUSIC_PLAYING) {
        SetTimer(launcher_window, SUDEKIMP_MUSIC_TIMER, 500u, NULL);
    } else {
        KillTimer(launcher_window, SUDEKIMP_MUSIC_TIMER);
    }
    {
        RECT bar = {SUDEKIMP_MUSIC_X + 104, SUDEKIMP_MUSIC_Y + 56,
                    SUDEKIMP_MUSIC_X + SUDEKIMP_ART_W - 8, SUDEKIMP_MUSIC_Y + 64};
        InvalidateRect(launcher_window, &bar, FALSE);
    }
}

/* Thin progress bar beside the buttons (position/length from MCI). */
static void paint_music_progress(HDC dc) {
    WCHAR position[32] = L"", length[32] = L"";
    RECT bar = {SUDEKIMP_MUSIC_X + 104, SUDEKIMP_MUSIC_Y + 58,
                SUDEKIMP_MUSIC_X + SUDEKIMP_ART_W - 8, SUDEKIMP_MUSIC_Y + 62};
    RECT done = bar;
    HBRUSH track = CreateSolidBrush(SUDEKIMP_COLOR_INPUT);
    HBRUSH fill = CreateSolidBrush(SUDEKIMP_COLOR_CYAN);
    long at = 0, total = 0;
    FillRect(dc, &bar, track);
    if (music_state == SUDEKIMP_MUSIC_PLAYING || music_state == SUDEKIMP_MUSIC_PAUSED) {
        if (mciSendStringW(L"status SudekiMPMusic position", position, 32u, NULL) == 0u &&
            mciSendStringW(L"status SudekiMPMusic length", length, 32u, NULL) == 0u) {
            at = wcstol(position, NULL, 10);
            total = wcstol(length, NULL, 10);
        }
        if (total > 0 && at >= 0) {
            done.right = done.left + MulDiv(bar.right - bar.left, at < total ? at : total, total);
            FillRect(dc, &done, fill);
        }
    }
    DeleteObject(track);
    DeleteObject(fill);
}

/* Play/Pause: start (download if needed), pause, or resume. */
static void toggle_music(HWND owner) {
    if (music_state == SUDEKIMP_MUSIC_PLAYING) {
        if (mciSendStringW(L"pause SudekiMPMusic", NULL, 0u, NULL) == 0u) {
            music_state = SUDEKIMP_MUSIC_PAUSED;
            set_status(L"Project music paused.");
        }
    } else if (music_state == SUDEKIMP_MUSIC_PAUSED) {
        if (mciSendStringW(L"resume SudekiMPMusic", NULL, 0u, NULL) == 0u ||
            mciSendStringW(L"play SudekiMPMusic notify", NULL, 0u, launcher_window) == 0u) {
            music_state = SUDEKIMP_MUSIC_PLAYING;
            set_status(L"Project music resumed.");
        }
    } else if (music_state == SUDEKIMP_MUSIC_STOPPED) {
        music_state = SUDEKIMP_MUSIC_LOADING;
        update_music_player();
        start_music_download(owner);
        return;
    }
    update_music_player();
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

static int control_tab(HWND window) {
    size_t index;
    for (index = 0u; index < launcher_control_count; ++index) {
        if (launcher_controls[index].window == window) {
            return launcher_controls[index].tab;
        }
    }
    return SUDEKIMP_TAB_ALWAYS;
}

static HWND add_control(int tab, HWND window) {
    if (window != NULL && launcher_control_count < SUDEKIMP_MAX_CONTROLS) {
        launcher_controls[launcher_control_count].window = window;
        launcher_controls[launcher_control_count].tab = tab;
        ++launcher_control_count;
    }
    return window;
}

static HWND create_child(int tab,
                         DWORD extended_style,
                         const WCHAR *class_name,
                         const WCHAR *text,
                         DWORD style,
                         int x,
                         int y,
                         int width,
                         int height,
                         int identifier) {
    HWND window = CreateWindowExW(extended_style,
                                  class_name,
                                  text,
                                  WS_CHILD | WS_VISIBLE | style,
                                  x,
                                  y,
                                  width,
                                  height,
                                  launcher_window,
                                  (HMENU)(INT_PTR)identifier,
                                  launcher_instance,
                                  NULL);
    apply_default_font(window);
    return add_control(tab, window);
}

static HWND create_label(int tab, const WCHAR *text, int x, int y, int width, int height,
                         int identifier, DWORD style) {
    return create_child(tab, 0u, L"STATIC", text, style, x, y, width, height, identifier);
}

static HWND create_button(int tab, const WCHAR *text, int x, int y, int width, int height,
                          int identifier) {
    return create_child(tab, 0u, L"BUTTON", text, WS_TABSTOP | BS_OWNERDRAW,
                        x, y, width, height, identifier);
}

static HWND create_checkbox(int tab, const WCHAR *text, int x, int y, int width, int height,
                            int identifier) {
    return create_child(tab, 0u, L"BUTTON", text, WS_TABSTOP | BS_AUTOCHECKBOX | BS_MULTILINE,
                        x, y, width, height, identifier);
}

static HWND create_combo(int tab, int x, int y, int width, int identifier) {
    return create_child(tab, 0u, L"COMBOBOX", L"",
                        WS_TABSTOP | WS_VSCROLL | CBS_DROPDOWNLIST,
                        x, y, width, 320, identifier);
}

static void update_cleanroom_lead_visibility(void);

static void show_tab(int tab) {
    size_t index;
    if (tab < 0 || tab >= SUDEKIMP_TAB_COUNT) {
        return;
    }
    active_tab = tab;
    if (tab == SUDEKIMP_TAB_MODS && mods_panel != NULL) {
        /* Recomputed on every visit: the folder may have changed on Play. */
        WCHAR game_directory[MAX_PATH];
        WCHAR config_path[MAX_PATH];
        GetWindowTextW(directory_edit, game_directory, MAX_PATH);
        trim_directory(game_directory);
        if (join_path(config_path, MAX_PATH, package_directory, L"SudekiMP.ini")) {
            SudekiMpModsPanelSetPaths(mods_panel, game_directory, config_path);
        }
    }
    for (index = 0u; index < launcher_control_count; ++index) {
        const int owner = launcher_controls[index].tab;
        ShowWindow(launcher_controls[index].window,
                   owner == SUDEKIMP_TAB_ALWAYS || owner == tab ||
                           (owner == SUDEKIMP_TAB_SIDEBAR && tab != SUDEKIMP_TAB_MODS) ?
                       SW_SHOW : SW_HIDE);
    }
    update_cleanroom_lead_visibility();
    /* Children too: the parent repaints its page surface under them. */
    RedrawWindow(launcher_window, NULL, NULL, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN);
}

/* "Start as" belongs to the Cleanroom profile only. */
static void update_cleanroom_lead_visibility(void) {
    const int profile = current_profile();
    size_t index;
    for (index = 0u; index < sizeof(lan_row) / sizeof(lan_row[0]); ++index) {
        if (lan_row[index] != NULL) {
            ShowWindow(lan_row[index],
                       active_tab == SUDEKIMP_TAB_PLAY && developer_mode ? SW_SHOW : SW_HIDE);
        }
    }
    const int show = active_tab == SUDEKIMP_TAB_PLAY && profile == SUDEKIMP_PROFILE_CLEANROOM ?
        SW_SHOW : SW_HIDE;
    const int show_mode = active_tab == SUDEKIMP_TAB_PLAY &&
        profile == SUDEKIMP_PROFILE_TITLE_MULTIPLAYER ? SW_SHOW : SW_HIDE;
    if (cleanroom_lead_label != NULL) ShowWindow(cleanroom_lead_label, show);
    if (cleanroom_lead_combo != NULL) ShowWindow(cleanroom_lead_combo, show);
    if (title_mode_label != NULL) ShowWindow(title_mode_label, show_mode);
    if (title_mode_combo != NULL) ShowWindow(title_mode_combo, show_mode);
}

static void release_launcher_art(void) {
    size_t index;
    for (index = 0u; index < SUDEKIMP_ART_COUNT; ++index) {
        if (launcher_art[index] != NULL) {
            DeleteObject(launcher_art[index]);
            launcher_art[index] = NULL;
        }
    }
}

/* The artwork stays in the player's own game installation; the launcher only
   reads the vanilla launcher bitmaps from <game>\launcherdata at runtime. */
static void reload_launcher_art(void) {
    static const WCHAR *const art_files[SUDEKIMP_ART_COUNT] = {
        L"SudekiPC_Launcher_Logo.bmp",
        L"SudekiPC_Launcher_Tal.bmp",
        L"SudekiPC_Launcher_Ailish.bmp",
        L"SudekiPC_Launcher_Buki.bmp",
        L"SudekiPC_Launcher_Elco.bmp",
        L"controller_map.bmp"
    };
    WCHAR game_directory[MAX_PATH];
    size_t index;
    release_launcher_art();
    if (selected_game_directory(game_directory, MAX_PATH)) {
        for (index = 0u; index < SUDEKIMP_ART_COUNT; ++index) {
            WCHAR path[MAX_PATH];
            if (SUCCEEDED(StringCchPrintfW(path, MAX_PATH, L"%s\\launcherdata\\%s",
                                           game_directory, art_files[index]))) {
                launcher_art[index] = (HBITMAP)LoadImageW(NULL, path, IMAGE_BITMAP, 0, 0,
                                                          LR_LOADFROMFILE);
            }
        }
    }
    InvalidateRect(launcher_window, NULL, TRUE);
}

static void set_options_source_text(void) {
    WCHAR path[MAX_PATH];
    WCHAR game_directory[MAX_PATH];
    const WCHAR *text;
    if (player_options_source == SUDEKIMP_OPTIONS_SOURCE_PLAYER) {
        text = L"Editing Sudeki's own options file (PlayerOptions.xml in your "
               L"AppData\\Roaming\\Sudeki folder).";
    } else if (player_options_source == SUDEKIMP_OPTIONS_SOURCE_DEFAULTS) {
        text = L"PlayerOptions.xml does not exist yet. These are the game's defaults; "
               L"Save creates the file.";
    } else if (SudekiMpOptionsStorePlayerPath(path, MAX_PATH) && file_exists(path)) {
        text = L"PlayerOptions.xml could not be read safely, so these options are "
               L"disabled and the file is left untouched.";
    } else if (!selected_game_directory(game_directory, MAX_PATH)) {
        text = L"Choose the Sudeki folder on the Play tab to load the game's options.";
    } else {
        text = L"Neither PlayerOptions.xml nor the game's default options could be read.";
    }
    if (options_source_label != NULL) {
        SetWindowTextW(options_source_label, text);
    }
}

static void load_player_options(void) {
    WCHAR game_directory[MAX_PATH];
    if (!selected_game_directory(game_directory, MAX_PATH)) {
        game_directory[0] = L'\0';
    }
    SudekiMpPlayerOptionsFree(&player_options);
    player_options_source = SudekiMpOptionsStoreLoad(game_directory, &player_options);
    populating_settings = TRUE;
    SudekiMpSettingsPopulate(&settings_controls,
                             player_options_source == SUDEKIMP_OPTIONS_SOURCE_NONE ?
                                 NULL : &player_options);
    populating_settings = FALSE;
    settings_dirty = FALSE;
    set_options_source_text();
}

static void on_game_directory_changed(void) {
    reload_launcher_art();
    if (player_options_source != SUDEKIMP_OPTIONS_SOURCE_PLAYER && !settings_dirty) {
        load_player_options();
    }
}

static void mark_settings_dirty(void) {
    if (!populating_settings) {
        settings_dirty = TRUE;
        set_status(L"Unsaved changes. Save or Play writes them.");
    }
}

static int selected_profile(void) {
    return current_profile();
}

static BOOL is_checked(HWND checkbox) {
    return checkbox != NULL && SendMessageW(checkbox, BM_GETCHECK, 0, 0) == BST_CHECKED;
}

static void update_mod_availability(void) {
    const int profile = selected_profile();
    const BOOL campaign = profile == SUDEKIMP_PROFILE_LOCAL_COOP ||
                          profile == SUDEKIMP_PROFILE_SAFE;
    const BOOL coop = profile == SUDEKIMP_PROFILE_LOCAL_COOP;
    EnableWindow(skip_movies_checkbox, campaign);
    EnableWindow(quick_menu_speed_checkbox, campaign);
    /* The DLL accepts the story boost only alongside the co-op roster menu. */
    EnableWindow(story_boost_checkbox, coop);
    EnableWindow(story_boost_combo, coop && is_checked(story_boost_checkbox));
}

static void select_story_boost_multiplier(const WCHAR *value) {
    int index = (int)SendMessageW(story_boost_combo, CB_GETCOUNT, 0, 0);
    while (--index >= 0) {
        if (lstrcmpW(story_boost_multipliers[index], value) == 0) {
            SendMessageW(story_boost_combo, CB_SETCURSEL, (WPARAM)index, 0);
            return;
        }
    }
    SendMessageW(story_boost_combo, CB_SETCURSEL, 1u, 0);
}

static void set_mod_defaults(void) {
    SendMessageW(skip_movies_checkbox, BM_SETCHECK, BST_CHECKED, 0);
    SendMessageW(quick_menu_speed_checkbox, BM_SETCHECK, BST_UNCHECKED, 0);
    SendMessageW(story_boost_checkbox, BM_SETCHECK, BST_UNCHECKED, 0);
    select_story_boost_multiplier(L"2.0");
    update_mod_availability();
}

static void load_mod_options(void) {
    WCHAR settings_path[MAX_PATH];
    WCHAR value[16];
    set_mod_defaults();
    if (!get_settings_path(settings_path, MAX_PATH)) {
        return;
    }
    GetPrivateProfileStringW(L"mods", L"skip_startup_movies", L"true", value, 16u,
                             settings_path);
    SendMessageW(skip_movies_checkbox, BM_SETCHECK,
                 lstrcmpiW(value, L"false") == 0 ? BST_UNCHECKED : BST_CHECKED, 0);
    GetPrivateProfileStringW(L"mods", L"quick_menu_normal_speed", L"false", value, 16u,
                             settings_path);
    SendMessageW(quick_menu_speed_checkbox, BM_SETCHECK,
                 lstrcmpiW(value, L"true") == 0 ? BST_CHECKED : BST_UNCHECKED, 0);
    GetPrivateProfileStringW(L"mods", L"story_test_boost", L"false", value, 16u,
                             settings_path);
    SendMessageW(story_boost_checkbox, BM_SETCHECK,
                 lstrcmpiW(value, L"true") == 0 ? BST_CHECKED : BST_UNCHECKED, 0);
    GetPrivateProfileStringW(L"mods", L"story_test_boost_multiplier", L"2.0", value, 16u,
                             settings_path);
    select_story_boost_multiplier(value);
    update_mod_availability();
}

static const WCHAR *selected_story_boost_multiplier(void) {
    const int index = (int)SendMessageW(story_boost_combo, CB_GETCURSEL, 0, 0);
    const int count = (int)(sizeof(story_boost_multipliers) /
                            sizeof(story_boost_multipliers[0]));
    return index >= 0 && index < count ? story_boost_multipliers[index] : L"2.0";
}

static BOOL persist_mod_options(void) {
    WCHAR settings_path[MAX_PATH];
    return get_settings_path(settings_path, MAX_PATH) &&
        WritePrivateProfileStringW(L"mods", L"skip_startup_movies",
            is_checked(skip_movies_checkbox) ? L"true" : L"false", settings_path) &&
        WritePrivateProfileStringW(L"mods", L"quick_menu_normal_speed",
            is_checked(quick_menu_speed_checkbox) ? L"true" : L"false", settings_path) &&
        WritePrivateProfileStringW(L"mods", L"story_test_boost",
            is_checked(story_boost_checkbox) ? L"true" : L"false", settings_path) &&
        WritePrivateProfileStringW(L"mods", L"story_test_boost_multiplier",
            selected_story_boost_multiplier(), settings_path);
}

/* Applies the mod checkboxes after the closed launch profile has been written.
   LAN arena and cleanroom profiles keep their fixed, validated key sets. */
static BOOL apply_mod_options(const WCHAR *config_path, SudekiMpLauncherProfile profile) {
    if (profile != SUDEKIMP_PROFILE_LOCAL_COOP && profile != SUDEKIMP_PROFILE_SAFE) {
        return TRUE;
    }
    if (!WritePrivateProfileStringW(L"SudekiMP", L"SkipStartupMovies",
            is_checked(skip_movies_checkbox) ? L"true" : L"false", config_path) ||
        !WritePrivateProfileStringW(L"SudekiMP", L"EnableQuickMenuNormalSpeed",
            is_checked(quick_menu_speed_checkbox) ? L"true" : L"false", config_path)) {
        return FALSE;
    }
    if (profile == SUDEKIMP_PROFILE_LOCAL_COOP && is_checked(story_boost_checkbox)) {
        return WritePrivateProfileStringW(L"SudekiMP", L"EnableStoryTestBoost", L"true",
                                          config_path) &&
               WritePrivateProfileStringW(L"SudekiMP", L"StoryTestBoostMultiplier",
                                          selected_story_boost_multiplier(), config_path);
    }
    return TRUE;
}

/* Edits a copy so a rejected value or failed write leaves the loaded
   document, and the file on disk, unchanged. */
static BOOL save_player_options(HWND owner) {
    SudekiMpPlayerOptions edited;
    unsigned char *bytes;
    size_t count;
    const char *failed_setting = NULL;
    BOOL loaded;
    /* Defaults are written even unchanged: Save is what creates the file. */
    if (player_options_source == SUDEKIMP_OPTIONS_SOURCE_NONE ||
        (player_options_source == SUDEKIMP_OPTIONS_SOURCE_PLAYER && !settings_dirty)) {
        return TRUE;
    }
    if (!SudekiMpPlayerOptionsSerialize(&player_options, &bytes, &count)) {
        show_error(owner, L"SudekiMP could not prepare Sudeki's options for saving.");
        return FALSE;
    }
    loaded = SudekiMpPlayerOptionsLoad(&edited, bytes, count);
    free(bytes);
    if (!loaded) {
        show_error(owner, L"SudekiMP could not prepare Sudeki's options for saving.");
        return FALSE;
    }
    if (!SudekiMpSettingsApply(&settings_controls, &edited, &failed_setting)) {
        WCHAR message[192];
        WCHAR setting[64];
        if (MultiByteToWideChar(CP_UTF8, 0, failed_setting != NULL ? failed_setting : "?",
                                -1, setting, 64) == 0) {
            setting[0] = L'\0';
        }
        StringCchPrintfW(message, 192,
                         L"The %s option could not be written, so nothing was saved.",
                         setting);
        show_error(owner, message);
        SudekiMpPlayerOptionsFree(&edited);
        return FALSE;
    }
    if (!SudekiMpOptionsStoreSave(&edited)) {
        show_error(owner,
                   L"SudekiMP could not save PlayerOptions.xml. The previous file was kept.");
        SudekiMpPlayerOptionsFree(&edited);
        return FALSE;
    }
    SudekiMpPlayerOptionsFree(&player_options);
    player_options = edited;
    player_options_source = SUDEKIMP_OPTIONS_SOURCE_PLAYER;
    set_options_source_text();
    return TRUE;
}

static BOOL save_all_settings(HWND owner) {
    persist_launcher_options();
    if (!persist_mod_options()) {
        show_error(owner, L"SudekiMP could not save its mod options.");
        return FALSE;
    }
    if (!save_player_options(owner)) {
        return FALSE;
    }
    settings_dirty = FALSE;
    set_status(player_options_source == SUDEKIMP_OPTIONS_SOURCE_NONE ?
        L"Launcher and mod options saved. Sudeki's own options are unavailable." :
        L"Settings saved.");
    return TRUE;
}

/* Copies the game's default value for each option on the current tab into a
   working copy that already holds every other tab's unsaved edits. */
static void restore_tab_defaults(HWND owner) {
    static const char *const video_settings[][2] = {
        {"Resolution", "Width"}, {"Resolution", "Height"}, {"Refresh", "Refresh"},
        {"AntiAliasing", "Value"}, {"FullScreen", "Value"}, {"Shadows", "Value"},
        {"Gamma", "Value"}, {"AudioQuality", "Value"}
    };
    static const char *const control_settings[][2] = {
        {"EnableMouse", "Value"}, {"InvertX", "Value"}, {"InvertY", "Value"},
        {"ForceFeedback", "Value"}
    };
    const char *const (*settings)[2];
    size_t setting_count;
    WCHAR game_directory[MAX_PATH];
    SudekiMpPlayerOptions defaults;
    SudekiMpPlayerOptions working;
    unsigned char *bytes;
    size_t count;
    const char *failed_setting;
    size_t index;

    if (active_tab == SUDEKIMP_TAB_TOOLS) {
        set_mod_defaults();
        mark_settings_dirty();
        return;
    }
    if (active_tab == SUDEKIMP_TAB_PLAY) {
        set_profile(SUDEKIMP_PROFILE_LOCAL_COOP);
        SetWindowTextW(lan_host_edit, L"127.0.0.1");
        SetWindowTextW(lan_port_edit, L"26770");
        SendMessageW(cleanroom_tools_checkbox, BM_SETCHECK, BST_CHECKED, 0);
        SendMessageW(auto_update_checkbox, BM_SETCHECK, BST_UNCHECKED, 0);
        persist_launcher_options();
        update_mod_availability();
        set_status(L"Play options reset to their defaults.");
        return;
    }
    if (active_tab == SUDEKIMP_TAB_VIDEO_AUDIO) {
        settings = video_settings;
        setting_count = sizeof(video_settings) / sizeof(video_settings[0]);
    } else if (active_tab == SUDEKIMP_TAB_CONTROLS) {
        settings = control_settings;
        setting_count = sizeof(control_settings) / sizeof(control_settings[0]);
    } else {
        set_status(L"This tab has no settings to reset.");
        return;
    }
    if (player_options_source == SUDEKIMP_OPTIONS_SOURCE_NONE) {
        show_error(owner, L"Sudeki's options are not loaded, so there is nothing to reset.");
        return;
    }
    if (!selected_game_directory(game_directory, MAX_PATH) ||
        !SudekiMpOptionsStoreLoadDefaults(game_directory, &defaults)) {
        show_error(owner, L"Choose the Sudeki folder on the Play tab first; the defaults "
                          L"come from the game's launcherdata folder.");
        return;
    }
    if (!SudekiMpPlayerOptionsSerialize(&player_options, &bytes, &count)) {
        SudekiMpPlayerOptionsFree(&defaults);
        return;
    }
    if (!SudekiMpPlayerOptionsLoad(&working, bytes, count)) {
        free(bytes);
        SudekiMpPlayerOptionsFree(&defaults);
        return;
    }
    free(bytes);
    if (SudekiMpSettingsApply(&settings_controls, &working, &failed_setting)) {
        for (index = 0u; index < setting_count; ++index) {
            char value[SUDEKIMP_PLAYER_OPTION_VALUE_CAPACITY];
            if (SudekiMpPlayerOptionsGet(&defaults, settings[index][0], settings[index][1],
                                         value, sizeof(value), NULL)) {
                (void)SudekiMpPlayerOptionsSet(&working, settings[index][0],
                                               settings[index][1], value);
            }
        }
        populating_settings = TRUE;
        SudekiMpSettingsPopulate(&settings_controls, &working);
        populating_settings = FALSE;
        mark_settings_dirty();
        set_status(L"Game defaults restored on this tab. Save or Play writes them.");
    }
    SudekiMpPlayerOptionsFree(&working);
    SudekiMpPlayerOptionsFree(&defaults);
}

static BOOL confirm_close(HWND owner) {
    int choice;
    if (!settings_dirty) {
        return TRUE;
    }
    choice = MessageBoxW(owner, L"Save your changes before quitting?", SUDEKIMP_TITLE,
                         MB_YESNOCANCEL | MB_ICONQUESTION);
    if (choice == IDCANCEL) {
        return FALSE;
    }
    return choice == IDNO || save_all_settings(owner);
}

static COLORREF button_fill_color(unsigned int identifier, BOOL selected) {
    if (identifier >= IDC_TAB_FIRST &&
        identifier < IDC_TAB_FIRST + (unsigned int)SUDEKIMP_TAB_COUNT) {
        if ((int)(identifier - IDC_TAB_FIRST) == active_tab) {
            return SUDEKIMP_COLOR_PANEL;
        }
        return selected ? RGB(53, 76, 101) : SUDEKIMP_COLOR_BUTTON;
    }
    if (identifier == IDC_LAUNCH) {
        return selected ? RGB(20, 132, 160) : SUDEKIMP_COLOR_CYAN;
    }
    if (identifier == IDC_SAVE) {
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
    const BOOL is_tab = draw->CtlID >= IDC_TAB_FIRST &&
        draw->CtlID < IDC_TAB_FIRST + (unsigned int)SUDEKIMP_TAB_COUNT;
    const BOOL active = is_tab && (int)(draw->CtlID - IDC_TAB_FIRST) == active_tab;
    COLORREF fill_color = disabled ? RGB(43, 52, 62) :
                                     button_fill_color(draw->CtlID, selected);

    /* RoundRect does not paint its corners. Clear the full owner-draw area
       with whatever surface the button sits on so nothing peeks through. */
    FillRect(draw->hDC, &content,
             control_tab(draw->hwndItem) == SUDEKIMP_TAB_ALWAYS &&
                     draw->CtlID != IDC_PLAY_MUSIC && draw->CtlID != IDC_STOP_MUSIC ?
                 app_background_brush : panel_background_brush);
    fill = CreateSolidBrush(fill_color);
    outline = CreatePen(PS_SOLID,
                        1,
                        draw->CtlID == IDC_LAUNCH ? RGB(145, 235, 245) :
                        active ? SUDEKIMP_COLOR_CYAN : RGB(77, 105, 133));
    old_brush = SelectObject(draw->hDC, fill);
    old_pen = SelectObject(draw->hDC, outline);
    RoundRect(draw->hDC,
              content.left,
              content.top,
              content.right,
              content.bottom + (active ? 8 : 0),
              8,
              8);
    SelectObject(draw->hDC, old_brush);
    SelectObject(draw->hDC, old_pen);
    DeleteObject(fill);
    DeleteObject(outline);

    if (draw->CtlID == IDC_PLAY_MUSIC || draw->CtlID == IDC_STOP_MUSIC) {
        /* Mini player icons: play triangle, pause bars, stop square. */
        const int cx = (content.left + content.right) / 2, cy = (content.top + content.bottom) / 2;
        HBRUSH icon = CreateSolidBrush(disabled ? RGB(125, 140, 155) : SUDEKIMP_COLOR_TEXT);
        HGDIOBJ previous_brush = SelectObject(draw->hDC, icon);
        HGDIOBJ previous_pen = SelectObject(draw->hDC, GetStockObject(NULL_PEN));
        if (draw->CtlID == IDC_STOP_MUSIC) {
            Rectangle(draw->hDC, cx - 5, cy - 5, cx + 6, cy + 6);
        } else if (music_state == SUDEKIMP_MUSIC_PLAYING) {
            Rectangle(draw->hDC, cx - 6, cy - 6, cx - 1, cy + 7);
            Rectangle(draw->hDC, cx + 2, cy - 6, cx + 7, cy + 7);
        } else {
            POINT triangle[3] = {{cx - 4, cy - 7}, {cx - 4, cy + 7}, {cx + 7, cy}};
            Polygon(draw->hDC, triangle, 3);
        }
        SelectObject(draw->hDC, previous_brush);
        SelectObject(draw->hDC, previous_pen);
        DeleteObject(icon);
        return;
    }
    GetWindowTextW(draw->hwndItem, text, (int)(sizeof(text) / sizeof(text[0])));
    SetBkMode(draw->hDC, TRANSPARENT);
    SetTextColor(draw->hDC,
                 disabled ? RGB(125, 140, 155) :
                 active ? SUDEKIMP_COLOR_CYAN : SUDEKIMP_COLOR_TEXT);
    old_font = SelectObject(draw->hDC,
                            body_font != NULL ? body_font : GetStockObject(DEFAULT_GUI_FONT));
    DrawTextW(draw->hDC,
              text,
              -1,
              &content,
              DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
    SelectObject(draw->hDC, old_font);
    if ((draw->itemState & ODS_FOCUS) != 0u) {
        InflateRect(&content, -3, -3);
        DrawFocusRect(draw->hDC, &content);
    }
}

static void paint_bitmap(HDC target, HBITMAP bitmap, int x, int y, int width, int height) {
    HDC source = CreateCompatibleDC(target);
    HGDIOBJ previous;
    BITMAP info;
    if (source == NULL) {
        return;
    }
    previous = SelectObject(source, bitmap);
    if (GetObjectW(bitmap, sizeof(info), &info) != 0) {
        BitBlt(target, x, y,
               info.bmWidth < width ? info.bmWidth : width,
               info.bmHeight < height ? info.bmHeight : height,
               source, 0, 0, SRCCOPY);
    }
    SelectObject(source, previous);
    DeleteDC(source);
}

static void paint_placeholder(HDC target, RECT *area, const WCHAR *text, HFONT font) {
    HGDIOBJ previous = SelectObject(target, font != NULL ? font : body_font);
    SetBkMode(target, TRANSPARENT);
    SetTextColor(target, SUDEKIMP_COLOR_MUTED);
    InflateRect(area, -14, -14);
    DrawTextW(target, text, -1, area, DT_CENTER | DT_WORDBREAK);
    SelectObject(target, previous);
}

static void paint_launcher(HWND window) {
    PAINTSTRUCT paint;
    HDC paint_dc = BeginPaint(window, &paint);
    RECT column = {SUDEKIMP_ART_X, SUDEKIMP_ART_Y,
                   SUDEKIMP_ART_X + SUDEKIMP_ART_W, SUDEKIMP_CLIENT_H - 16};
    RECT page = {SUDEKIMP_PAGE_X, SUDEKIMP_PAGE_Y,
                 SUDEKIMP_PAGE_X + SUDEKIMP_PAGE_W, SUDEKIMP_PAGE_Y + SUDEKIMP_PAGE_H};
    RECT status = {SUDEKIMP_PAGE_X, SUDEKIMP_STATUS_Y,
                   SUDEKIMP_PAGE_X + SUDEKIMP_PAGE_W, SUDEKIMP_STATUS_Y + 30};
    RECT logo = {SUDEKIMP_ART_X, SUDEKIMP_ART_Y,
                 SUDEKIMP_ART_X + SUDEKIMP_ART_W, SUDEKIMP_ART_Y + SUDEKIMP_LOGO_H};
    RECT portrait = {SUDEKIMP_ART_X, SUDEKIMP_PORTRAIT_Y,
                     SUDEKIMP_ART_X + SUDEKIMP_ART_W,
                     SUDEKIMP_PORTRAIT_Y + SUDEKIMP_PORTRAIT_H};
    const HBITMAP portrait_art = launcher_art[tab_art[active_tab]];

    if (active_tab == SUDEKIMP_TAB_MODS) {
        /* The workshop covers the art column; the logo moves beside the
           status bar and a title takes the corner left of the tabs. */
        RECT title = {SUDEKIMP_ART_X, SUDEKIMP_ART_Y, SUDEKIMP_PAGE_X - 8, SUDEKIMP_PAGE_Y - 4};
        HGDIOBJ old_font;
        /* The workshop paints itself; filling its rectangle here would erase
           its child controls, which this parent does not clip. */
        FillRect(paint_dc, &status, panel_background_brush);
        SetBkMode(paint_dc, TRANSPARENT);
        SetTextColor(paint_dc, SUDEKIMP_COLOR_CYAN);
        old_font = SelectObject(paint_dc, title_font);
        DrawTextW(paint_dc, L"Mod workshop", -1, &title,
                  DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        SelectObject(paint_dc, old_font);
        {
            /* The mini player keeps its panel below the workshop. */
            RECT player = {SUDEKIMP_ART_X, SUDEKIMP_MUSIC_Y,
                           SUDEKIMP_ART_X + SUDEKIMP_ART_W, SUDEKIMP_CLIENT_H - 16};
            FillRect(paint_dc, &player, panel_background_brush);
        }
        paint_music_progress(paint_dc);
        EndPaint(window, &paint);
        return;
    }
    FillRect(paint_dc, &column, panel_background_brush);
    FillRect(paint_dc, &page, panel_background_brush);
    FillRect(paint_dc, &status, panel_background_brush);
    if (launcher_art[SUDEKIMP_ART_LOGO] != NULL) {
        paint_bitmap(paint_dc, launcher_art[SUDEKIMP_ART_LOGO], logo.left, logo.top,
                     SUDEKIMP_ART_W, SUDEKIMP_LOGO_H);
    } else {
        logo.top += 26;
        paint_placeholder(paint_dc, &logo, L"SUDEKI", title_font);
    }
    if (portrait_art != NULL) {
        paint_bitmap(paint_dc, portrait_art, portrait.left, portrait.top,
                     SUDEKIMP_ART_W, SUDEKIMP_PORTRAIT_H);
    } else {
        portrait.top += 110;
        paint_placeholder(paint_dc, &portrait,
                          L"Choose the Sudeki folder on the Play tab to show the game's "
                          L"launcher art.", NULL);
    }
    paint_music_progress(paint_dc);
    if (active_tab == SUDEKIMP_TAB_CONTROLLER) {
        RECT map = {SUDEKIMP_CONTENT_X, SUDEKIMP_CONTENT_Y,
                    SUDEKIMP_CONTENT_X + 289, SUDEKIMP_CONTENT_Y + 360};
        if (launcher_art[SUDEKIMP_ART_CONTROLLER_MAP] != NULL) {
            paint_bitmap(paint_dc, launcher_art[SUDEKIMP_ART_CONTROLLER_MAP],
                         map.left, map.top, 289, 360);
        } else {
            map.top += 150;
            paint_placeholder(paint_dc, &map,
                              L"The controller map comes from the game folder.", NULL);
        }
    }
    EndPaint(window, &paint);
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
            const int control_id = GetDlgCtrlID((HWND)lparam);
            SetTextColor(control_dc,
                         control_id == IDC_STATUS || control_id == IDC_NOTE ||
                                 control_id == IDC_MUSIC_STATE ||
                                 control_id == IDC_OPTIONS_SOURCE ?
                             SUDEKIMP_COLOR_MUTED :
                         control_id == IDC_SAVE_WARNING ? RGB(244, 105, 105) :
                         control_id == IDC_HEADING || control_id == IDC_MUSIC_TITLE ?
                             SUDEKIMP_COLOR_CYAN :
                                                     SUDEKIMP_COLOR_TEXT);
            /* Every label, checkbox and slider sits on a painted panel, so an
               opaque panel brush also clears old text when a label changes. */
            SetBkColor(control_dc, SUDEKIMP_COLOR_PANEL);
            return (LRESULT)panel_background_brush;
        }
        case WM_CTLCOLOREDIT:
            SetTextColor((HDC)wparam, SUDEKIMP_COLOR_TEXT);
            SetBkColor((HDC)wparam, SUDEKIMP_COLOR_INPUT);
            return (LRESULT)input_background_brush;
        case WM_PAINT:
            paint_launcher(window);
            return 0;
        case WM_HSCROLL:
            if ((HWND)lparam == settings_controls.gamma) {
                SudekiMpSettingsUpdateGammaLabel(&settings_controls);
                mark_settings_dirty();
                return 0;
            }
            break;
        case WM_COMMAND: {
            const unsigned int identifier = LOWORD(wparam);
            const unsigned int notification = HIWORD(wparam);
            if (identifier >= IDC_TAB_FIRST &&
                identifier < IDC_TAB_FIRST + (unsigned int)SUDEKIMP_TAB_COUNT) {
                show_tab((int)(identifier - IDC_TAB_FIRST));
                return 0;
            }
            switch (identifier) {
                case IDC_BROWSE:
                    browse_for_game_directory(window);
                    return 0;
                case IDC_GAME_DIRECTORY:
                    if (notification == EN_KILLFOCUS) {
                        on_game_directory_changed();
                    }
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
                case IDC_SAVE:
                    (void)save_all_settings(window);
                    return 0;
                case IDC_DEFAULT:
                    restore_tab_defaults(window);
                    return 0;
                case IDC_QUIT:
                    SendMessageW(window, WM_CLOSE, 0, 0);
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
                    persist_launcher_options();
                    update_mod_availability();
                    update_cleanroom_lead_visibility();
                    return 0;
                case IDC_DEVELOPER_MODE:
                    developer_mode = is_checked(developer_mode_checkbox);
                    rebuild_profile_list();
                    persist_launcher_options();
                    update_mod_availability();
                    update_cleanroom_lead_visibility();
                    set_status(developer_mode ?
                        L"Developer mode on: LAN arena test profiles are on the Play tab." :
                        L"Developer mode off: LAN arena test profiles are hidden.");
                    return 0;
                case IDC_CLEANROOM_LEAD:
                case IDC_TITLE_MODE:
                    if (notification == CBN_SELCHANGE) {
                        persist_launcher_options();
                    }
                    return 0;
                case IDC_LAN_HOST:
                case IDC_LAN_PORT:
                case IDC_AUTO_UPDATE:
                case IDC_CLEANROOM_TOOLS:
                    persist_launcher_options();
                    return 0;
                case IDC_RESOLUTION:
                    if (notification == CBN_SELCHANGE) {
                        SudekiMpSettingsResolutionChanged(&settings_controls);
                        mark_settings_dirty();
                    }
                    return 0;
                case IDC_REFRESH:
                case IDC_ANTIALIASING:
                case IDC_AUDIO_QUALITY:
                case IDC_STORY_BOOST_MULTIPLIER:
                    if (notification == CBN_SELCHANGE) {
                        mark_settings_dirty();
                    }
                    return 0;
                case IDC_STORY_BOOST:
                    update_mod_availability();
                    mark_settings_dirty();
                    return 0;
                case IDC_FULLSCREEN:
                case IDC_SHADOWS:
                case IDC_MOUSE:
                case IDC_INVERT_X:
                case IDC_INVERT_Y:
                case IDC_FORCE_FEEDBACK:
                case IDC_SKIP_MOVIES:
                case IDC_QUICK_MENU_SPEED:
                    mark_settings_dirty();
                    return 0;
                case IDC_INSTALL_COOP_SAVES:
                    (void)install_coop_save_fixtures(window);
                    return 0;
                case IDC_TEST_XINPUT:
                    test_xinput_controller(window);
                    return 0;
                case IDC_PLAY_MUSIC:
                    toggle_music(window);
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
        }
        case WM_TIMER:
            if (wparam == SUDEKIMP_MUSIC_TIMER) {
                update_music_player();
                return 0;
            }
            break;
        case MM_MCINOTIFY:
            /* The track reached its end (superseded/aborted come from pause/stop). */
            if (wparam == MCI_NOTIFY_SUCCESSFUL && music_state == SUDEKIMP_MUSIC_PLAYING) {
                close_music();
                set_status(L"Project music finished.");
            }
            return 0;
        case WM_SUDEKIMP_MUSIC_COMPLETE:
            if (wparam != 0u && music_state != SUDEKIMP_MUSIC_LOADING) {
                /* Stopped (or the game started) while the track was fetched. */
                update_music_player();
            } else if (wparam != 0u) {
                (void)play_cached_music(window);
            } else {
                music_state = SUDEKIMP_MUSIC_STOPPED;
                update_music_player();
                show_error(window,
                           L"The project music catalog or track could not be downloaded. "
                           L"The launcher never changed the game installation.");
                set_status(L"Project music is unavailable. Check your connection and try again.");
            }
            return 0;
        case WM_CLOSE:
            if (confirm_close(window)) {
                DestroyWindow(window);
            }
            return 0;
        case WM_ACTIVATE:
            /* Back from Explorer: pick up mod folders copied in meanwhile. */
            if (LOWORD(wparam) != WA_INACTIVE && active_tab == SUDEKIMP_TAB_MODS) {
                show_tab(SUDEKIMP_TAB_MODS);
            }
            break;
        case WM_DESTROY:
            if (mods_panel != NULL) {
                SudekiMpModsPanelDestroy(mods_panel);
                mods_panel = NULL;
            }
            persist_launcher_options();
            close_music();
            if (launched_game_job != NULL) {
                CloseHandle(launched_game_job);
                launched_game_job = NULL;
            }
            SudekiMpPlayerOptionsFree(&player_options);
            release_launcher_art();
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

    {
        INITCOMMONCONTROLSEX common_controls;
        common_controls.dwSize = sizeof(common_controls);
        common_controls.dwICC = ICC_BAR_CLASSES | ICC_LISTVIEW_CLASSES;
        InitCommonControlsEx(&common_controls);
    }
    {
        RECT frame = {0, 0, SUDEKIMP_CLIENT_W, SUDEKIMP_CLIENT_H};
        const DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
        AdjustWindowRectEx(&frame, style, FALSE, 0u);
        launcher_window = CreateWindowExW(0u,
                                           window_class.lpszClassName,
                                           SUDEKIMP_TITLE,
                                           style,
                                           CW_USEDEFAULT,
                                           CW_USEDEFAULT,
                                           frame.right - frame.left,
                                           frame.bottom - frame.top,
                                           NULL,
                                           NULL,
                                           instance,
                                           NULL);
    }
    if (launcher_window == NULL) {
        return 1;
    }
    icon = LoadIconW(instance, MAKEINTRESOURCEW(IDI_SUDEKIMP_BETA));
    displayed_icon = (HICON)LoadImageW(instance,
                                       MAKEINTRESOURCEW(IDI_SUDEKIMP_BETA),
                                       IMAGE_ICON,
                                       48,
                                       48,
                                       LR_DEFAULTCOLOR);
    SendMessageW(launcher_window, WM_SETICON, ICON_BIG, (LPARAM)icon);
    SendMessageW(launcher_window, WM_SETICON, ICON_SMALL, (LPARAM)icon);

    /* Left column: game art is painted above; the project identity sits below. */
    control = create_label(SUDEKIMP_TAB_SIDEBAR, L"", SUDEKIMP_ART_X + 4,
                           SUDEKIMP_IDENTITY_Y, 48, 48, IDC_PROJECT_ICON, SS_ICON);
    SendMessageW(control,
                 STM_SETICON,
                 (WPARAM)(displayed_icon != NULL ? displayed_icon : icon),
                 0);
    control = create_label(SUDEKIMP_TAB_SIDEBAR, L"SUDEKI TOGETHER", SUDEKIMP_ART_X + 58,
                           SUDEKIMP_IDENTITY_Y + 2, 160, 24, 0, SS_NOPREFIX);
    control = create_label(SUDEKIMP_TAB_SIDEBAR, L"Launcher " SUDEKIMP_LAUNCHER_VERSION,
                           SUDEKIMP_ART_X + 58, SUDEKIMP_IDENTITY_Y + 26, 160, 20,
                           IDC_NOTE, 0u);
    apply_font(control, subtitle_font);
    control = create_label(SUDEKIMP_TAB_SIDEBAR,
                           L"Local co-op • LAN arena • cleanroom • exact-build validation",
                           SUDEKIMP_ART_X + 8, SUDEKIMP_IDENTITY_Y + 58, 204, 40,
                           IDC_NOTE, 0u);
    apply_font(control, subtitle_font);

    {
        static const WCHAR *const tab_titles[SUDEKIMP_TAB_COUNT] = {
            L"Play", L"Video/Audio", L"Controls", L"Controller", L"Mods", L"Tools"
        };
        int tab;
        for (tab = 0; tab < SUDEKIMP_TAB_COUNT; ++tab) {
            create_button(SUDEKIMP_TAB_ALWAYS, tab_titles[tab],
                          SUDEKIMP_PAGE_X + tab * 105, SUDEKIMP_ART_Y, 101, 32,
                          IDC_TAB_FIRST + tab);
        }
    }

    {
        const int x = SUDEKIMP_CONTENT_X;
        const int y = SUDEKIMP_CONTENT_Y;
        const int tab = SUDEKIMP_TAB_PLAY;
        create_label(tab, L"Game", x, y, 300, 22, IDC_HEADING, 0u);
        create_label(tab, L"Sudeki folder (the folder that contains SUDEKI.exe):",
                     x, y + 30, SUDEKIMP_CONTENT_W, 20, 0, 0u);
        directory_edit = create_child(tab, WS_EX_CLIENTEDGE, L"EDIT", L"",
                                      WS_TABSTOP | ES_AUTOHSCROLL,
                                      x, y + 54, 466, 30, IDC_GAME_DIRECTORY);
        create_button(tab, L"Browse…", x + 478, y + 53, 118, 32, IDC_BROWSE);
        create_label(tab, L"Launch", x, y + 104, 300, 22, IDC_HEADING, 0u);
        create_label(tab, L"Profile:", x, y + 138, 70, 22, 0, 0u);
        profile_combo = create_combo(tab, x + 76, y + 134, 320, IDC_PROFILE);
        rebuild_profile_list();
        cleanroom_lead_label = create_label(tab, L"Start as:", x + 410, y + 138, 70, 22, 0, 0u);
        cleanroom_lead_combo = create_combo(tab, x + 484, y + 134, 112, IDC_CLEANROOM_LEAD);
        {
            size_t index;
            for (index = 0u; index < sizeof(cleanroom_lead_names) / sizeof(cleanroom_lead_names[0]);
                 ++index) {
                SendMessageW(cleanroom_lead_combo, CB_ADDSTRING, 0, (LPARAM)cleanroom_lead_names[index]);
            }
            SendMessageW(cleanroom_lead_combo, CB_SETCURSEL, 0, 0);
        }
        /* Same spot as "Start as"; only one of the two is ever shown. */
        title_mode_label = create_label(tab, L"Mode:", x + 410, y + 138, 70, 22, 0, 0u);
        title_mode_combo = create_combo(tab, x + 484, y + 134, 112, IDC_TITLE_MODE);
        {
            size_t index;
            for (index = 0u; index < sizeof(title_mode_names) / sizeof(title_mode_names[0]);
                 ++index) {
                SendMessageW(title_mode_combo, CB_ADDSTRING, 0, (LPARAM)title_mode_names[index]);
            }
            SendMessageW(title_mode_combo, CB_SETCURSEL, 0, 0);
        }
        lan_row[0] = create_label(tab, L"LAN IP:", x, y + 178, 70, 22, 0, 0u);
        lan_host_edit = create_child(tab, WS_EX_CLIENTEDGE, L"EDIT", L"127.0.0.1",
                                     WS_TABSTOP | ES_AUTOHSCROLL,
                                     x + 76, y + 174, 170, 28, IDC_LAN_HOST);
        lan_row[1] = create_label(tab, L"Port:", x + 262, y + 178, 44, 22, 0, 0u);
        lan_port_edit = create_child(tab, WS_EX_CLIENTEDGE, L"EDIT", L"26770",
                                     WS_TABSTOP | ES_NUMBER,
                                     x + 310, y + 174, 86, 28, IDC_LAN_PORT);
        lan_row[2] = lan_host_edit;
        lan_row[3] = lan_port_edit;
        cleanroom_tools_checkbox = create_checkbox(
            tab,
            L"Enable cleanroom sandbox tools (F8): actors, dummy, combat/camera, inventory, "
            L"infinite meters",
            x, y + 216, SUDEKIMP_CONTENT_W, 40, IDC_CLEANROOM_TOOLS);
        SendMessageW(cleanroom_tools_checkbox, BM_SETCHECK, BST_CHECKED, 0);
        auto_update_checkbox = create_checkbox(
            tab, L"Check for updates on startup (prompt before download)",
            x, y + 262, SUDEKIMP_CONTENT_W, 22, IDC_AUTO_UPDATE);
        create_button(tab, L"Verify build", x, y + 304, 160, 36, IDC_VERIFY);
        create_label(tab,
                     L"Play saves every tab, writes the selected closed profile, verifies the "
                     L"exact supported GOG build, then starts the game. LAN arena and "
                     L"cleanroom do not read campaign saves.",
                     x, y + 360, SUDEKIMP_CONTENT_W, 60, IDC_NOTE, 0u);
    }

    {
        static const WCHAR *const row_labels[] = {
            L"Resolution", L"Refresh rate", L"Anti-aliasing", L"Full screen", L"Shadows",
            L"Gamma"
        };
        const int x = SUDEKIMP_CONTENT_X;
        const int y = SUDEKIMP_CONTENT_Y;
        const int field_x = x + 196;
        const int tab = SUDEKIMP_TAB_VIDEO_AUDIO;
        int row;
        create_label(tab, L"Video", x, y, 300, 22, IDC_HEADING, 0u);
        for (row = 0; row < (int)(sizeof(row_labels) / sizeof(row_labels[0])); ++row) {
            create_label(tab, row_labels[row], x, y + 36 + row * 36, 180, 22, 0, SS_RIGHT);
        }
        settings_controls.resolution = create_combo(tab, field_x, y + 32, 220, IDC_RESOLUTION);
        settings_controls.refresh = create_combo(tab, field_x, y + 68, 220, IDC_REFRESH);
        settings_controls.antialiasing =
            create_combo(tab, field_x, y + 104, 220, IDC_ANTIALIASING);
        settings_controls.fullscreen =
            create_checkbox(tab, L"", field_x, y + 144, 24, 22, IDC_FULLSCREEN);
        settings_controls.shadows =
            create_checkbox(tab, L"", field_x, y + 180, 24, 22, IDC_SHADOWS);
        settings_controls.gamma = create_child(tab, 0u, TRACKBAR_CLASSW, L"",
                                               WS_TABSTOP | TBS_HORZ | TBS_AUTOTICKS,
                                               field_x - 6, y + 210, 272, 32, IDC_GAMMA);
        settings_controls.gamma_value =
            create_label(tab, L"1.00", field_x + 274, y + 216, 60, 22, IDC_GAMMA_VALUE, 0u);
        create_label(tab, L"Audio", x, y + 262, 300, 22, IDC_HEADING, 0u);
        create_label(tab, L"Audio quality", x, y + 298, 180, 22, 0, SS_RIGHT);
        settings_controls.audio_quality =
            create_combo(tab, field_x, y + 294, 220, IDC_AUDIO_QUALITY);
        options_source_label = create_label(tab, L"", x, y + 348, SUDEKIMP_CONTENT_W, 44,
                                            IDC_OPTIONS_SOURCE, 0u);
        create_label(tab,
                     L"These are Sudeki's own launcher options, the same ones its original "
                     L"launcher edits. Options the file does not contain stay disabled.",
                     x, y + 400, SUDEKIMP_CONTENT_W, 44, IDC_NOTE, 0u);
    }

    {
        const int x = SUDEKIMP_CONTENT_X;
        const int y = SUDEKIMP_CONTENT_Y;
        const int tab = SUDEKIMP_TAB_CONTROLS;
        create_label(tab, L"Camera and feedback", x, y, 300, 22, IDC_HEADING, 0u);
        settings_controls.mouse =
            create_checkbox(tab, L"Mouse enabled", x, y + 30, 150, 22, IDC_MOUSE);
        settings_controls.invert_x =
            create_checkbox(tab, L"Invert X", x + 160, y + 30, 110, 22, IDC_INVERT_X);
        settings_controls.invert_y =
            create_checkbox(tab, L"Invert Y", x + 280, y + 30, 110, 22, IDC_INVERT_Y);
        settings_controls.force_feedback = create_checkbox(
            tab, L"Force feedback", x + 400, y + 30, 160, 22, IDC_FORCE_FEEDBACK);
        create_label(tab, L"Key bindings", x, y + 70, 300, 22, IDC_HEADING, 0u);
        settings_controls.bindings = create_child(
            tab, WS_EX_CLIENTEDGE, WC_LISTVIEWW, L"",
            WS_TABSTOP | LVS_REPORT | LVS_SINGLESEL | LVS_NOSORTHEADER | LVS_SHOWSELALWAYS,
            x, y + 98, SUDEKIMP_CONTENT_W, 300, IDC_BINDINGS);
        ListView_SetExtendedListViewStyle(settings_controls.bindings,
                                          LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);
        ListView_SetBkColor(settings_controls.bindings, SUDEKIMP_COLOR_INPUT);
        ListView_SetTextBkColor(settings_controls.bindings, SUDEKIMP_COLOR_INPUT);
        ListView_SetTextColor(settings_controls.bindings, SUDEKIMP_COLOR_TEXT);
        SudekiMpSettingsInitialiseBindingColumns(settings_controls.bindings);
        create_label(tab,
                     L"Bindings are shown read-only. Rebinding is not available in this "
                     L"launcher yet.",
                     x, y + 408, SUDEKIMP_CONTENT_W, 40, IDC_NOTE, 0u);
    }

    {
        const int x = SUDEKIMP_CONTENT_X + 305;
        const int y = SUDEKIMP_CONTENT_Y;
        const int tab = SUDEKIMP_TAB_CONTROLLER;
        create_label(tab, L"Controller", x, y, 290, 22, IDC_HEADING, 0u);
        create_label(tab,
                     L"Sudeki's controller layout, from the game's own launcher. In local "
                     L"co-op, Player 2 uses the first XInput controller (slot 0).",
                     x, y + 30, 290, 100, 0, 0u);
        create_button(tab, L"Test XInput controller…", x, y + 140, 230, 34,
                      IDC_TEST_XINPUT);
    }

    {
        static const struct {
            const WCHAR *text;
            int identifier;
        } tools[] = {
            {L"Stop tracked game", IDC_STOP_GAME},
            {L"View runtime log", IDC_VIEW_LOG},
            {L"Export support logs…", IDC_EXPORT_LOGS},
            {L"Check for updates…", IDC_UPDATE},
            {L"Install co-op save fixtures…", IDC_INSTALL_COOP_SAVES},
            {L"Developer: wander", IDC_DEVELOPER}
        };
        const int x = SUDEKIMP_CONTENT_X;
        const int y = SUDEKIMP_CONTENT_Y;
        const int tab = SUDEKIMP_TAB_TOOLS;
        size_t index;
        create_label(tab, L"Tools", x, y, 300, 22, IDC_HEADING, 0u);
        for (index = 0u; index < sizeof(tools) / sizeof(tools[0]); ++index) {
            create_button(tab, tools[index].text,
                          x + (int)(index % 2u) * 306, y + 32 + (int)(index / 2u) * 46,
                          290, 34, tools[index].identifier);
        }
        create_label(tab,
                     L"LAN arena and cleanroom do not read campaign saves. Support logs stay "
                     L"on this PC until you share them; nothing is uploaded.",
                     x, y + 432, SUDEKIMP_CONTENT_W, 40, IDC_SAVE_WARNING, 0u);
    }

    {
        static const WCHAR *const multiplier_labels[] = {
            L"1.5x", L"2x", L"3x (experimental)", L"4x (experimental)"
        };
        const int x = SUDEKIMP_CONTENT_X;
        const int y = SUDEKIMP_CONTENT_Y + 214;
        const int tab = SUDEKIMP_TAB_TOOLS;
        size_t index;
        create_label(tab, L"SudekiMP options", x, y, 300, 22, IDC_HEADING, 0u);
        skip_movies_checkbox = create_checkbox(
            tab, L"Skip the publisher and logo movies at startup (story movies still play)",
            x, y + 32, SUDEKIMP_CONTENT_W, 22, IDC_SKIP_MOVIES);
        quick_menu_speed_checkbox = create_checkbox(
            tab, L"Keep normal game speed while the Quick Menu is open",
            x, y + 64, SUDEKIMP_CONTENT_W, 22, IDC_QUICK_MENU_SPEED);
        story_boost_checkbox = create_checkbox(
            tab, L"Story test boost: F6 toggles faster world time and invulnerability",
            x, y + 96, 420, 40, IDC_STORY_BOOST);
        story_boost_combo = create_combo(tab, x + 430, y + 100, 166,
                                         IDC_STORY_BOOST_MULTIPLIER);
        for (index = 0u; index < sizeof(multiplier_labels) / sizeof(multiplier_labels[0]);
             ++index) {
            SendMessageW(story_boost_combo, CB_ADDSTRING, 0, (LPARAM)multiplier_labels[index]);
        }
        create_label(tab,
                     L"Applied when you press Play. Local co-op uses all three; Safe launch "
                     L"uses the first two; LAN arena and cleanroom keep their fixed "
                     L"profiles. The story boost always starts off until you press F6.",
                     x, y + 136, SUDEKIMP_CONTENT_W, 48, IDC_NOTE, 0u);
        developer_mode_checkbox = create_checkbox(
            tab, L"Developer mode: show the LAN arena test profiles on Play",
            x, y + 190, SUDEKIMP_CONTENT_W, 22, IDC_DEVELOPER_MODE);
    }

    status_label = create_label(SUDEKIMP_TAB_ALWAYS,
                                L"Paste or choose the supported GOG Sudeki folder, then verify it.",
                                SUDEKIMP_PAGE_X + 12, SUDEKIMP_STATUS_Y + 5,
                                SUDEKIMP_PAGE_W - 24, 22, IDC_STATUS, SS_ENDELLIPSIS);
    {
        static const struct {
            const WCHAR *text;
            int identifier;
        } actions[] = {
            {L"Save", IDC_SAVE}, {L"Play", IDC_LAUNCH},
            {L"Default", IDC_DEFAULT}, {L"Quit", IDC_QUIT}
        };
        size_t index;
        for (index = 0u; index < sizeof(actions) / sizeof(actions[0]); ++index) {
            create_button(SUDEKIMP_TAB_ALWAYS, actions[index].text,
                          SUDEKIMP_PAGE_X + (int)index * 160, SUDEKIMP_ACTIONS_Y,
                          148, 36, actions[index].identifier);
        }
    }

    /* Mini player: title, state, Play/Pause, Stop, progress (painted). */
    music_title_label = create_label(SUDEKIMP_TAB_ALWAYS, L"Map Inversion",
                                     SUDEKIMP_MUSIC_X + 10, SUDEKIMP_MUSIC_Y + 6,
                                     SUDEKIMP_ART_W - 20, 20, IDC_MUSIC_TITLE,
                                     SS_ENDELLIPSIS | SS_NOPREFIX);
    music_state_label = create_label(SUDEKIMP_TAB_ALWAYS, L"Stopped",
                                     SUDEKIMP_MUSIC_X + 10, SUDEKIMP_MUSIC_Y + 26,
                                     SUDEKIMP_ART_W - 20, 18, IDC_MUSIC_STATE, 0u);
    apply_font(music_state_label, subtitle_font);
    music_play_button = create_button(SUDEKIMP_TAB_ALWAYS, L"", SUDEKIMP_MUSIC_X + 10,
                                      SUDEKIMP_MUSIC_Y + 46, 40, 28, IDC_PLAY_MUSIC);
    music_stop_button = create_button(SUDEKIMP_TAB_ALWAYS, L"", SUDEKIMP_MUSIC_X + 56,
                                      SUDEKIMP_MUSIC_Y + 46, 40, 28, IDC_STOP_MUSIC);
    /* Track name only; nothing is downloaded until Play. */
    (void)get_music_cache_path(music_cache_path,
                               sizeof(music_cache_path) / sizeof(music_cache_path[0]));
    update_music_player();
    mods_panel = SudekiMpModsPanelCreate(launcher_window, launcher_instance);
    if (mods_panel != NULL) {
        MoveWindow(mods_panel, SUDEKIMP_MODS_X, SUDEKIMP_PAGE_Y, SUDEKIMP_MODS_W,
                   SUDEKIMP_PAGE_H, FALSE);
        SudekiMpModsPanelSetStyle(mods_panel, body_font, body_font);
        SudekiMpModsPanelSetStatusLabel(mods_panel, status_label);
        add_control(SUDEKIMP_TAB_MODS, mods_panel);
    }
    load_saved_game_directory();
    load_mod_options();
    reload_launcher_art();
    load_player_options();
    show_tab(SUDEKIMP_TAB_PLAY);
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
