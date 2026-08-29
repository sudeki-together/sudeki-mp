#define _WIN32_WINNT 0x0600

#include <windows.h>
#include <commctrl.h>
#include <mmsystem.h>
#include <shellapi.h>
#include <shlobj.h>
#include <strsafe.h>
#include <urlmon.h>

#include <string.h>
#include <wchar.h>

#include "beta_launcher_resource.h"

#define SUDEKIMP_TITLE L"SudekiMP Windows Beta Launcher"
#define SUDEKIMP_PROJECT_URL L"https://git.unfilteredrealm.com/wander"
#define SUDEKIMP_MUSIC_MANIFEST_URL \
    L"https://git.unfilteredrealm.com/sudeki-together/sudeki-mp/raw/branch/main/public/music/manifest.txt"
#define SUDEKIMP_MUSIC_TRACK_URL \
    L"https://git.unfilteredrealm.com/sudeki-together/sudeki-mp/raw/branch/main/public/music/Map%20Inversion.mp3"

static HINSTANCE launcher_instance;
static HWND launcher_window;
static HWND directory_edit;
static HWND status_label;
static WCHAR package_directory[MAX_PATH];
static WCHAR music_cache_path[MAX_PATH];
static LONG music_download_running;
static HBRUSH app_background_brush;
static HBRUSH panel_background_brush;
static HBRUSH input_background_brush;
static HFONT body_font;
static HFONT title_font;
static HFONT subtitle_font;

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
                                 BOOL check_only) {
    WCHAR game_executable[MAX_PATH];
    if (!join_path(game_executable, MAX_PATH, game_directory, L"SUDEKI.exe")) {
        return FALSE;
    }
    return SUCCEEDED(StringCchPrintfW(command,
                                      command_count,
                                      check_only
                                          ? L"\"%s\" --check \"%s\" \"%s\""
                                          : L"\"%s\" \"%s\" \"%s\"",
                                      loader_path,
                                      game_executable,
                                      dll_path));
}

static BOOL verify_game(HWND owner) {
    WCHAR game_directory[MAX_PATH];
    WCHAR loader_path[MAX_PATH];
    WCHAR dll_path[MAX_PATH];
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
                              TRUE)) {
        return FALSE;
    }
    ZeroMemory(&startup, sizeof(startup));
    ZeroMemory(&process, sizeof(process));
    startup.cb = sizeof(startup);
    set_status(L"Verifying the selected GOG game build…");
    RedrawWindow(launcher_window, NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW);
    if (!CreateProcessW(NULL,
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

static void launch_game(HWND owner) {
    WCHAR game_directory[MAX_PATH];
    WCHAR loader_path[MAX_PATH];
    WCHAR dll_path[MAX_PATH];
    WCHAR command[MAX_PATH * 3u + 80u];
    STARTUPINFOW startup;
    PROCESS_INFORMATION process;

    if (!verify_game(owner) ||
        !build_loader_command(command,
                              sizeof(command) / sizeof(command[0]),
                              loader_path,
                              game_directory,
                              dll_path,
                              FALSE)) {
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
                        CREATE_NEW_CONSOLE,
                        NULL,
                        game_directory,
                        &startup,
                        &process)) {
        show_error(owner, L"SudekiMP could not start the loader.");
        set_status(L"Launch failed before injection.");
        return;
    }
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    set_status(L"SudekiMP loader started. Keep its console open if it reports an error.");
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

static void begin_optional_update(HWND owner) {
    WCHAR updater_path[MAX_PATH];
    WCHAR parameters[MAX_PATH + 80u];
    const INT_PTR result = MessageBoxW(
        owner,
        L"Manual package downloads are recommended. This opt-in action contacts the "
        L"Sudeki Together server over HTTPS, downloads an unsigned beta ZIP, and "
        L"updates only SudekiMP files. It preserves SudekiMP.ini and never changes "
        L"SUDEKI.exe, game data, or saves.\n\nContinue?",
        L"Optional SudekiMP beta update",
        MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2);

    if (result != IDYES) {
        set_status(L"Update cancelled. No files changed.");
        return;
    }
    if (!join_path(updater_path,
                   sizeof(updater_path) / sizeof(updater_path[0]),
                   package_directory,
                   L"SudekiMP-Beta-Launcher.ps1") ||
        !file_exists(updater_path) ||
        FAILED(StringCchPrintfW(parameters,
                                sizeof(parameters) / sizeof(parameters[0]),
                                L"-NoProfile -ExecutionPolicy Bypass -File \"%s\" -UpdateOnly -WaitForPid %lu",
                                updater_path,
                                (unsigned long)GetCurrentProcessId()))) {
        show_error(owner, L"The packaged update helper is missing.");
        return;
    }
    if ((INT_PTR)ShellExecuteW(owner,
                               L"open",
                               L"powershell.exe",
                               parameters,
                               package_directory,
                               SW_HIDE) <= 32) {
        show_error(owner, L"Windows could not start the optional update helper.");
        return;
    }
    set_status(L"The update helper will continue after this launcher closes.");
    DestroyWindow(owner);
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
            SetTextColor(control_dc,
                         GetDlgCtrlID(control) == IDC_STATUS ? SUDEKIMP_COLOR_MUTED :
                                                                SUDEKIMP_COLOR_TEXT);
            SetBkMode(control_dc, TRANSPARENT);
            if (GetDlgCtrlID(control) == IDC_STATUS) {
                return (LRESULT)panel_background_brush;
            }
            return (LRESULT)app_background_brush;
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
                    (void)verify_game(window);
                    return 0;
                case IDC_LAUNCH:
                    launch_game(window);
                    return 0;
                case IDC_UPDATE:
                    begin_optional_update(window);
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
            close_music();
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
                                       450,
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
                            L"Windows beta launcher • validates before injection • game files stay untouched",
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
    control = CreateWindowW(L"BUTTON",
                            L"Verify build",
                            WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
                            28,
                            252,
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
                            252,
                            190,
                            38,
                            launcher_window,
                            (HMENU)(INT_PTR)IDC_LAUNCH,
                            instance,
                            NULL);
    apply_default_font(control);
    control = CreateWindowW(L"BUTTON",
                            L"Optional update…",
                            WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
                            392,
                            252,
                            170,
                            38,
                            launcher_window,
                            (HMENU)(INT_PTR)IDC_UPDATE,
                            instance,
                            NULL);
    apply_default_font(control);
    control = CreateWindowW(L"BUTTON",
                            L"Play project music",
                            WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
                            28,
                            316,
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
                            316,
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
                            316,
                            192,
                            34,
                            launcher_window,
                            (HMENU)(INT_PTR)IDC_DEVELOPER,
                            instance,
                            NULL);
    apply_default_font(control);
    control = CreateWindowW(L"STATIC",
                            L"Music is fetched only when you press Play, cached under LocalAppData, and played inside this launcher.",
                            WS_CHILD | WS_VISIBLE,
                            28,
                            382,
                            680,
                            28,
                            launcher_window,
                            NULL,
                            instance,
                            NULL);
    apply_default_font(control);

    load_saved_game_directory();
    ShowWindow(launcher_window, show_command);
    UpdateWindow(launcher_window);
    while (GetMessageW(&message, NULL, 0u, 0u) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    return (int)message.wParam;
}
