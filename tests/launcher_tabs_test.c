/* Synthetic launcher UI regression: no game, network actions, or injection. */
#define wWinMain SudekiMpBetaLauncherMain
#include "../src/launcher/beta_launcher.c"
#undef wWinMain
#include <assert.h>
#include <stdio.h>

static void click_tab(HWND window, int tab) {
    SendMessageW(window, WM_COMMAND, MAKEWPARAM(IDC_TAB_FIRST + tab, BN_CLICKED),
                 (LPARAM)GetDlgItem(window, IDC_TAB_FIRST + tab));
}
static DWORD WINAPI exercise_tabs(void *unused) {
    HWND window = NULL, pane = NULL, launch, profile, directory, skip_movies, identity;
    RECT pane_rect;
    WCHAR text[128];
    DWORD deadline = GetTickCount() + 15000;
    int i;
    (void)unused;
    do {
        window = FindWindowW(L"SudekiMPBetaLauncher", NULL);
        if (window && IsWindowVisible(window))
            pane = FindWindowExW(window, NULL, L"SudekiMPModsPanel", NULL);
        if (pane) break;
        Sleep(10);
    } while ((LONG)(deadline - GetTickCount()) > 0);
    assert(window && pane);
    launch = GetDlgItem(window, IDC_LAUNCH);
    profile = GetDlgItem(window, IDC_PROFILE);
    directory = GetDlgItem(window, IDC_GAME_DIRECTORY);
    skip_movies = GetDlgItem(window, IDC_SKIP_MOVIES);
    identity = GetDlgItem(window, IDC_PROJECT_ICON);
    if (!(launch && profile && directory && skip_movies && identity))
        { HWND c; printf("missing launch=%p profile=%p directory=%p skip=%p identity=%p\n", (void *)launch, (void *)profile, (void *)directory, (void *)skip_movies, (void *)identity);
          for (c = GetWindow(window, GW_CHILD); c; c = GetWindow(c, GW_HWNDNEXT)) { WCHAR t[64]; GetWindowTextW(c, t, 64); printf("  id=%d %ls\n", GetDlgCtrlID(c), t); } }
    assert(launch && profile && directory && skip_movies && identity);
    /* Play first: the workshop is hidden, the art column is shown. */
    assert(IsWindowVisible(directory) && !IsWindowVisible(pane) && IsWindowVisible(identity));
    /* Developer mode off: no LAN arena profiles and no LAN address row. */
    assert(SendMessageW(profile, CB_GETCOUNT, 0, 0) == 5);
    assert(!IsWindowVisible(GetDlgItem(window, IDC_LAN_HOST)));
    SendMessageW(GetDlgItem(window, IDC_DEVELOPER_MODE), BM_SETCHECK, BST_CHECKED, 0);
    SendMessageW(window, WM_COMMAND, MAKEWPARAM(IDC_DEVELOPER_MODE, BN_CLICKED),
                 (LPARAM)GetDlgItem(window, IDC_DEVELOPER_MODE));
    assert(SendMessageW(profile, CB_GETCOUNT, 0, 0) == 7);
    assert(IsWindowVisible(GetDlgItem(window, IDC_LAN_HOST)));
    set_profile(SUDEKIMP_PROFILE_LAN_HOST);
    SendMessageW(GetDlgItem(window, IDC_DEVELOPER_MODE), BM_SETCHECK, BST_UNCHECKED, 0);
    SendMessageW(window, WM_COMMAND, MAKEWPARAM(IDC_DEVELOPER_MODE, BN_CLICKED),
                 (LPARAM)GetDlgItem(window, IDC_DEVELOPER_MODE));
    assert(SendMessageW(profile, CB_GETCOUNT, 0, 0) == 5 &&
           current_profile() == SUDEKIMP_PROFILE_LOCAL_COOP);
    set_profile(SUDEKIMP_PROFILE_SAFE);
    SetWindowTextW(directory, L"synthetic-invalid-install");
    for (i = 0; i < 12; ++i) {
        click_tab(window, SUDEKIMP_TAB_MODS);
        /* Mods: full-width workshop, no art column, Play's controls hidden,
           the shared action row (Play/Save/...) still available. */
        assert(IsWindowVisible(pane) && !IsWindowVisible(directory) && !IsWindowVisible(identity));
        assert(IsWindowVisible(launch));
        GetWindowRect(pane, &pane_rect);
        MapWindowPoints(NULL, window, (POINT *)&pane_rect, 2);
        assert(pane_rect.left == SUDEKIMP_MODS_X && pane_rect.right == SUDEKIMP_MODS_X + SUDEKIMP_MODS_W);
        /* The SudekiMP options moved to Tools. */
        click_tab(window, SUDEKIMP_TAB_TOOLS);
        assert(IsWindowVisible(skip_movies) && !IsWindowVisible(pane) && IsWindowVisible(identity));
        click_tab(window, SUDEKIMP_TAB_PLAY);
        assert(!IsWindowVisible(pane) && IsWindowVisible(directory));
        assert(current_profile() == SUDEKIMP_PROFILE_SAFE);
        GetWindowTextW(directory, text, 128);
        assert(!wcscmp(text, L"synthetic-invalid-install"));
    }
    /* "Start as" appears only for the Cleanroom profile and reaches the game. */
    {
        HWND lead = GetDlgItem(window, IDC_CLEANROOM_LEAD);
        WCHAR command[2048];
        assert(lead);
        set_profile(SUDEKIMP_PROFILE_LOCAL_COOP);
        SendMessageW(window, WM_COMMAND, MAKEWPARAM(IDC_PROFILE, CBN_SELCHANGE), (LPARAM)profile);
        assert(!IsWindowVisible(lead));
        set_profile(SUDEKIMP_PROFILE_CLEANROOM);
        SendMessageW(window, WM_COMMAND, MAKEWPARAM(IDC_PROFILE, CBN_SELCHANGE), (LPARAM)profile);
        assert(IsWindowVisible(lead));
        click_tab(window, SUDEKIMP_TAB_MODS);
        assert(!IsWindowVisible(lead));
        click_tab(window, SUDEKIMP_TAB_PLAY);
        assert(IsWindowVisible(lead));
        SendMessageW(lead, CB_SETCURSEL, 1, 0); /* Tal */
        assert(build_loader_command(command, 2048, L"L.exe", L"C:\\Game", L"M.dll", FALSE,
                                    SUDEKIMP_PROFILE_CLEANROOM));
        assert(wcsstr(command, L"--game-arg=-Level --game-arg=testroom") &&
               wcsstr(command, L"--game-arg=-Tal --game-arg=1") && !wcsstr(command, L"-Ailish"));
        SendMessageW(lead, CB_SETCURSEL, 0, 0);
    }
    /* Title-menu multiplayer turns on the saved-story [TitleMenu] lobby (no
       extra choice beside the profile); every other profile turns it off. */
    {
        HWND lead = GetDlgItem(window, IDC_CLEANROOM_LEAD);
        WCHAR saved_package[MAX_PATH], ini[MAX_PATH], value[32];
        HANDLE file;
        DWORD written;
        static const char seed[] = "[SudekiMP]\r\nEnableCleanroomMenu=true\r\n";
        set_profile(SUDEKIMP_PROFILE_TITLE_MULTIPLAYER);
        SendMessageW(window, WM_COMMAND, MAKEWPARAM(IDC_PROFILE, CBN_SELCHANGE), (LPARAM)profile);
        assert(!IsWindowVisible(lead));
        StringCchCopyW(saved_package, MAX_PATH, package_directory);
        GetEnvironmentVariableW(L"LOCALAPPDATA", package_directory, MAX_PATH);
        StringCchPrintfW(ini, MAX_PATH, L"%ls\\SudekiMP.ini", package_directory);
        file = CreateFileW(ini, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
        assert(file != INVALID_HANDLE_VALUE && WriteFile(file, seed, sizeof(seed) - 1, &written, NULL));
        CloseHandle(file);
        assert(configure_launcher_profile(window, SUDEKIMP_PROFILE_TITLE_MULTIPLAYER));
        GetPrivateProfileStringW(L"TitleMenu", L"Enabled", L"", value, 32, ini); assert(!wcscmp(value, L"true"));
        GetPrivateProfileStringW(L"TitleMenu", L"Scope", L"", value, 32, ini); assert(!wcscmp(value, L"saved-story"));
        GetPrivateProfileStringW(L"SudekiMP", L"EnableCleanroomMenu", L"", value, 32, ini); assert(!wcscmp(value, L"false"));
        /* Local host + client: the package ini gets the title-menu profile but
           never [Lobby] keys; each role folder gets a DLL+ini copy with its keys. */
        {
            WCHAR dll[MAX_PATH], host_dir[MAX_PATH], client_dir[MAX_PATH], role_ini[MAX_PATH];
            WCHAR command[MAX_PATH * 3u + 80u], section[64];
            StringCchPrintfW(dll, MAX_PATH, L"%ls\\SudekiMP.dll", package_directory);
            file = CreateFileW(dll, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
            assert(file != INVALID_HANDLE_VALUE && WriteFile(file, "MZ", 2, &written, NULL));
            CloseHandle(file);
            assert(configure_launcher_profile(window, SUDEKIMP_PROFILE_LOCAL_PAIR));
            GetPrivateProfileStringW(L"TitleMenu", L"Enabled", L"", value, 32, ini); assert(!wcscmp(value, L"true"));
            assert(GetPrivateProfileSectionW(L"Lobby", section, 64, ini) == 0);
            assert(stage_local_pair_role(L"host", L"SudekiMP HOST", TRUE, L"26770", host_dir));
            assert(stage_local_pair_role(L"client", L"SudekiMP CLIENT", FALSE, L"26770", client_dir));
            assert(wcsstr(host_dir, L"\\SudekiMP\\instances\\host") && wcsstr(client_dir, L"\\instances\\client"));
            StringCchPrintfW(role_ini, MAX_PATH, L"%ls\\SudekiMP.ini", host_dir);
            GetPrivateProfileStringW(L"SudekiMP", L"WindowTitle", L"", value, 32, role_ini); assert(!wcscmp(value, L"SudekiMP HOST"));
            GetPrivateProfileStringW(L"TitleMenu", L"AllowSecondInstance", L"", value, 32, role_ini); assert(!wcscmp(value, L"true"));
            GetPrivateProfileStringW(L"TitleMenu", L"Scope", L"", value, 32, role_ini); assert(!wcscmp(value, L"saved-story"));
            GetPrivateProfileStringW(L"Lobby", L"AutoHost", L"", value, 32, role_ini); assert(!wcscmp(value, L"true"));
            GetPrivateProfileStringW(L"Lobby", L"AutoJoin", L"", value, 32, role_ini); assert(!value[0]);
            StringCchPrintfW(role_ini, MAX_PATH, L"%ls\\SudekiMP.ini", client_dir);
            GetPrivateProfileStringW(L"SudekiMP", L"WindowTitle", L"", value, 32, role_ini); assert(!wcscmp(value, L"SudekiMP CLIENT"));
            GetPrivateProfileStringW(L"Lobby", L"AutoJoin", L"", value, 32, role_ini); assert(!wcscmp(value, L"127.0.0.1"));
            GetPrivateProfileStringW(L"Lobby", L"AutoHost", L"", value, 32, role_ini); assert(!value[0]);
            GetPrivateProfileStringW(L"Lobby", L"Port", L"", value, 32, role_ini); assert(!wcscmp(value, L"26770"));
            {   /* Only the client gets its own profile folder (saves/options copy). */
                WCHAR folder[MAX_PATH];
                GetPrivateProfileStringW(L"TitleMenu", L"ProfileFolder", L"", folder, MAX_PATH, role_ini);
                assert(wcsstr(folder, L"\\instances\\client\\Profile") && GetFileAttributesW(folder) != INVALID_FILE_ATTRIBUTES);
                StringCchPrintfW(folder, MAX_PATH, L"%ls\\SudekiMP.ini", host_dir);
                GetPrivateProfileStringW(L"TitleMenu", L"ProfileFolder", L"", value, 32, folder); assert(!value[0]);
            }
            StringCchPrintfW(role_ini, MAX_PATH, L"%ls\\SudekiMP.dll", client_dir);
            assert(GetFileAttributesW(role_ini) != INVALID_FILE_ATTRIBUTES);
            /* The client's loader command points at the client's staged DLL. */
            assert(build_loader_command(command, sizeof(command) / sizeof(command[0]), L"Loader.exe",
                                        L"C:\\Game", role_ini, FALSE, SUDEKIMP_PROFILE_LOCAL_PAIR));
            assert(wcsstr(command, L"\\instances\\client\\SudekiMP.dll") && !wcsstr(command, L"--game-arg"));
            DeleteFileW(dll);
        }
        assert(configure_launcher_profile(window, SUDEKIMP_PROFILE_SAFE));
        GetPrivateProfileStringW(L"TitleMenu", L"Enabled", L"", value, 32, ini); assert(!wcscmp(value, L"false"));
        /* Flight: greyed out with its instruction until Safe launch is chosen;
           then written to [Flight] with the chosen speed; off for other profiles. */
        {
            HWND flight = GetDlgItem(window, IDC_FLIGHT), speed = GetDlgItem(window, IDC_FLIGHT_SPEED);
            assert(flight && speed && !IsWindowEnabled(flight) && !IsWindowEnabled(speed));
            GetWindowTextW(flight_note, value, 32);
            assert(!wcsncmp(value, L"Available only with Safe launch", 31));
            set_profile(SUDEKIMP_PROFILE_SAFE);
            SendMessageW(window, WM_COMMAND, MAKEWPARAM(IDC_PROFILE, CBN_SELCHANGE), (LPARAM)profile);
            assert(IsWindowEnabled(flight) && !IsWindowEnabled(speed));
            SendMessageW(flight, BM_SETCHECK, BST_CHECKED, 0);
            SendMessageW(window, WM_COMMAND, MAKEWPARAM(IDC_FLIGHT, BN_CLICKED), (LPARAM)flight);
            assert(IsWindowEnabled(speed));
            SendMessageW(speed, CB_SETCURSEL, 3, 0); /* Very fast */
            assert(configure_launcher_profile(window, SUDEKIMP_PROFILE_SAFE));
            GetPrivateProfileStringW(L"Flight", L"Enabled", L"", value, 32, ini); assert(!wcscmp(value, L"true"));
            GetPrivateProfileStringW(L"Flight", L"Speed", L"", value, 32, ini); assert(!wcscmp(value, L"24"));
            GetPrivateProfileStringW(L"Flight", L"ForwardSpeed", L"", value, 32, ini); assert(!wcscmp(value, L"4"));
            GetPrivateProfileStringW(L"Flight", L"AilishOnly", L"", value, 32, ini); assert(!wcscmp(value, L"false"));
            assert(configure_launcher_profile(window, SUDEKIMP_PROFILE_CLEANROOM));
            GetPrivateProfileStringW(L"Flight", L"Enabled", L"", value, 32, ini); assert(!wcscmp(value, L"false"));
            SendMessageW(flight, BM_SETCHECK, BST_UNCHECKED, 0);
            SendMessageW(window, WM_COMMAND, MAKEWPARAM(IDC_FLIGHT, BN_CLICKED), (LPARAM)flight);
        }
        DeleteFileW(ini);
        StringCchCopyW(package_directory, MAX_PATH, saved_package);
    }
    PostMessageW(window, WM_CLOSE, 0, 0);
    return 0;
}
int wmain(void) {
    WCHAR temp[MAX_PATH], settings[MAX_PATH], file[MAX_PATH];
    HANDLE worker;
    int result;
    GetTempPathW(MAX_PATH, temp);
    StringCchPrintfW(settings, MAX_PATH, L"%lsSudekiMP-tabs-test-%lu", temp, (unsigned long)GetCurrentProcessId());
    assert(CreateDirectoryW(settings, NULL));
    assert(SetEnvironmentVariableW(L"LOCALAPPDATA", settings));
    worker = CreateThread(NULL, 0, exercise_tabs, NULL, 0, NULL);
    assert(worker);
    result = SudekiMpBetaLauncherMain(GetModuleHandleW(NULL), NULL, L"", SW_SHOW);
    assert(WaitForSingleObject(worker, 15000) == WAIT_OBJECT_0);
    CloseHandle(worker);
    StringCchPrintfW(file, MAX_PATH, L"%ls\\SudekiMP\\windows-beta-launcher.ini", settings);
    DeleteFileW(file);
    StringCchPrintfW(file, MAX_PATH, L"%ls\\SudekiMP\\thumbs", settings);
    RemoveDirectoryW(file);
    StringCchPrintfW(file, MAX_PATH, L"%ls\\SudekiMP\\music", settings);
    RemoveDirectoryW(file);
    StringCchPrintfW(file, MAX_PATH, L"%ls\\SudekiMP", settings);
    RemoveDirectoryW(file);
    RemoveDirectoryW(settings);
    assert(result == 0);
    puts("LauncherTabsTest: passed (Mods workshop full width, options on Tools, Play state kept, Cleanroom start-as, developer mode)");
    return 0;
}
