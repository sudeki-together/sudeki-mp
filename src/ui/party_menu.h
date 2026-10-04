#ifndef SUDEKIMP_PARTY_MENU_H
#define SUDEKIMP_PARTY_MENU_H
#include "ui/title_menu_view.h"

/* Plain presentation state. The command owner validates every operation again
 * on the game thread; an enabled button never grants native authority. */
typedef enum SudekiMpPartyMenuAction {
    SUDEKIMP_PARTY_MENU_OPEN, SUDEKIMP_PARTY_MENU_CLOSE,
    SUDEKIMP_PARTY_MENU_AWAY, SUDEKIMP_PARTY_MENU_RETURN,
    SUDEKIMP_PARTY_MENU_SWITCH, SUDEKIMP_PARTY_MENU_RELEASE,
    SUDEKIMP_PARTY_MENU_REQUEST_PAUSE, SUDEKIMP_PARTY_MENU_PAUSE,
    SUDEKIMP_PARTY_MENU_RESUME, SUDEKIMP_PARTY_MENU_POLICY,
    SUDEKIMP_PARTY_MENU_LEAVE,
    /* Explicit host transfer: argument=(recipient_player<<8)|character.
     * Player and canonical character are each 0..3; no native authority here. */
    SUDEKIMP_PARTY_MENU_REASSIGN
} SudekiMpPartyMenuAction;
typedef struct SudekiMpPartyMenuState {
    unsigned local_player;
    uint8_t connected, reserved, controlling, input_ready, menu, away;
    uint8_t character[4], busy_characters, policy, paused;
    BOOL active, host, combat, transition;
    char name[4][32], status[128];
    /* Saved-story presentation has no transfer/pause authority. It exposes
     * only the session/player pages and local close/leave operations. */
    BOOL saved_story;
    char story_detail[4][96];
} SudekiMpPartyMenuState;
typedef BOOL (*SudekiMpPartyMenuCommand)(SudekiMpPartyMenuAction, unsigned);
void SudekiMpPartyMenuInitialize(SudekiMpPartyMenuCommand);
void SudekiMpPartyMenuStateSet(const SudekiMpPartyMenuState *);
void SudekiMpPartyMenuToggle(void);
BOOL SudekiMpPartyMenuCapturesInput(void);
/* Called only by the exact native menu render adapter, on its game thread. */
BOOL SudekiMpPartyMenuRender(void *device);
/* Does not release the shared title renderer: its existing owner does that. */
void SudekiMpPartyMenuReset(void);
#endif
