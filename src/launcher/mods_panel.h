#ifndef SUDEKIMP_MODS_PANEL_H
#define SUDEKIMP_MODS_PANEL_H
#include <windows.h>

/* The panel owns its workers and child controls. Paths are copied; calling
 * SetPaths again cancels an old scan and starts one for the new directory.
 * ini_path is the configuration the launcher will pass to the game. */
HWND SudekiMpModsPanelCreate(HWND parent, HINSTANCE instance);
void SudekiMpModsPanelSetPaths(HWND panel, const WCHAR *game_directory,
                               const WCHAR *ini_path);
void SudekiMpModsPanelDestroy(HWND panel);
/* Launcher integration: borrowed fonts (the caller keeps them alive for the
 * panel's lifetime) and the launcher's status bar for the panel's messages. */
void SudekiMpModsPanelSetStyle(HWND panel, HFONT body_font, HFONT heading_font);
void SudekiMpModsPanelSetStatusLabel(HWND panel, HWND label);
#endif
