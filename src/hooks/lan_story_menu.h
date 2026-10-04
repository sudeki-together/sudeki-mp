#ifndef SUDEKIMP_LAN_STORY_MENU_H
#define SUDEKIMP_LAN_STORY_MENU_H
#include "network/title_lobby.h"
#include "hooks/lan_story_observer.h"

/* Runtime owns the single MenuNative hook, client containment, host input
 * fence and positive session drain. This adapter owns UI and scoped host
 * resource preferences. Every call stays on the initializing native thread. */
BOOL SudekiMpLanStoryMenuInitialize(HMODULE image,unsigned local_player);
/* Borrowed only during this exact native dispatch. No queued native work. */
void SudekiMpLanStoryMenuServiceTools(void *controller,
    const SudekiMpControlUpdateDispatchWitness *,const SudekiMpLanStoryScene *,
    BOOL host_commands_ready);
/* Restore only session-owned resource flags; never refill on exit. The
 * renderer continues retrying a failed state-block restoration until drain. */
BOOL SudekiMpLanStoryMenuDrainTools(void);
BOOL SudekiMpLanStoryMenuDrainToolsOnDispatch(void *controller,
    const SudekiMpControlUpdateDispatchWitness *,const SudekiMpLanStoryScene *);
void SudekiMpLanStoryMenuRefresh(const SudekiMpLobbyStatus *,
    unsigned leader_character,unsigned available_mask,BOOL scene_ready,
    BOOL host_control_ready,BOOL spectator_playback_ready,BOOL client_control_ready);
void SudekiMpLanStoryMenuNotice(const char *text);
void SudekiMpLanStoryMenuToggle(void);
BOOL SudekiMpLanStoryMenuRender(void *device);
/* These describe local UI intent, never an actor/input authority lease. */
BOOL SudekiMpLanStoryMenuCapturesInput(void);
BOOL SudekiMpLanStoryMenuLeaveRequested(void);
/* Call only after the owning MenuNative callback has been retired. Shared
 * title-renderer resources are left with their existing renderer owner. */
BOOL SudekiMpLanStoryMenuReset(void);
#endif
