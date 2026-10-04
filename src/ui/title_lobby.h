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
BOOL SudekiMpLobbyUiBack(unsigned *selection);
#endif
