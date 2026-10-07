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
    assert(SendMessageW(profile, CB_GETCOUNT, 0, 0) == 5);
    SendMessageW(profile, CB_SETCURSEL, SUDEKIMP_PROFILE_SAFE, 0);
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
        assert(SendMessageW(profile, CB_GETCURSEL, 0, 0) == SUDEKIMP_PROFILE_SAFE);
        GetWindowTextW(directory, text, 128);
        assert(!wcscmp(text, L"synthetic-invalid-install"));
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
    StringCchPrintfW(file, MAX_PATH, L"%ls\\SudekiMP", settings);
    RemoveDirectoryW(file);
    RemoveDirectoryW(settings);
    assert(result == 0);
    puts("LauncherTabsTest: passed (Mods workshop full width, options on Tools, Play state kept)");
    return 0;
}
