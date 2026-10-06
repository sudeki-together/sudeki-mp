#ifndef SUDEKIMP_TITLE_LOBBY_UI_H
#define SUDEKIMP_TITLE_LOBBY_UI_H
#include "ui/title_menu_view.h"
#include "network/title_lobby.h"
/* All calls on the verified title thread except Destroy after callbacks drain.
 * The worker owns sockets/plain selections; native ownership is separate. */
typedef struct SudekiMpLobbyView {
    unsigned count, enabled;
    SudekiMpTitleExtras text;
} SudekiMpLobbyView;
void SudekiMpLobbyUiOpen(void);
void SudekiMpLobbyUiClose(void);
BOOL SudekiMpLobbyUiDestroy(void);
void SudekiMpLobbyUiPoll(HWND window, BOOL input);
/* Kept alive on the same UI thread after the native title closes. */
void SudekiMpLobbyUiBackground(void);
/* Retained after title loading; game-thread adapters submit confirmed state. */
SudekiMpLobby *SudekiMpLobbyUiSession(void);
const SudekiMpLobbyView *SudekiMpLobbyUiView(void);
BOOL SudekiMpLobbyUiEditing(void);
void SudekiMpLobbyUiArm(unsigned row);
/* Pointer field switching finishes editing without requiring Enter first. */
void SudekiMpLobbyUiEndEdit(void);
unsigned SudekiMpLobbyUiPageRevision(void);
/* TRUE returns to the native root; selection receives a valid new-page row. */
BOOL SudekiMpLobbyUiCommit(unsigned *selection);
/* Optional ini-driven automation of the same lobby actions a player would
 * take (create/join, character, saved game, ready, start). Copy-only config;
 * the driver runs on the title thread from Poll and never bypasses the
 * session, save verification or start admission paths. */
typedef struct SudekiMpLobbyAuto {
    BOOL host,join,ready,start;
    char room[32],address[32],name[32]; /* name: player name to set once in the room */
    uint16_t port;
    unsigned save_slot;    /* host: SAVESLOTdddd suffix, ~0u = none */
    unsigned character;    /* 0 Buki,1 Elco,2 Tal,3 Ailish, 4 = keep */
    unsigned min_players;  /* host auto start waits for this many members */
} SudekiMpLobbyAuto;
void SudekiMpLobbyUiConfigureAuto(const SudekiMpLobbyAuto *config);
/* TRUE until the automated page has been opened once. */
BOOL SudekiMpLobbyUiAutoWantsOpen(void);
BOOL SudekiMpLobbyUiBack(unsigned *selection);
#endif
