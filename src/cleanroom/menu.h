#ifndef SUDEKIMP_CLEANROOM_MENU_H
#define SUDEKIMP_CLEANROOM_MENU_H

#include <windows.h>

BOOL SudekiMpInstallCleanroomMenu(HMODULE game_module, UINT toggle_key);
enum { SUDEKIMP_PARTY_TOOL_COMBAT, SUDEKIMP_PARTY_TOOL_INFINITE_SP,
    SUDEKIMP_PARTY_TOOL_TRAINING_SKILLS, SUDEKIMP_PARTY_TOOL_INFINITE_SPIRIT,
    SUDEKIMP_PARTY_TOOL_INFINITE_JETPACK, SUDEKIMP_PARTY_TOOL_CAMERA,
    SUDEKIMP_PARTY_TOOL_FUEL_CRYSTAL,
    SUDEKIMP_PARTY_TOOL_FLIGHT_LEDGE, SUDEKIMP_PARTY_TOOL_HITBOXES, SUDEKIMP_PARTY_TOOL_CLOSE,
    SUDEKIMP_PARTY_TOOL_COUNT };
/* Presenter only: borrows the already initialized engine and existing
 * game-thread update/render callbacks. Installs no native hooks and does not
 * initialize/reset the engine, actors, audio, or any legacy menu subsystem. */
BOOL SudekiMpInstallLanPartyToolsMenu(HMODULE game_module,UINT toggle_key,
    unsigned local_seat, BOOL (*command)(unsigned action),
    BOOL (*query)(unsigned action,BOOL *enabled),
    void (*connection_status)(char *text,unsigned capacity));
/* Same presenter and resource ownership; story rows omit Test Room setup. */
BOOL SudekiMpInstallLanStoryToolsMenu(HMODULE game_module,UINT toggle_key,
    unsigned local_seat, BOOL (*command)(unsigned action),
    BOOL (*query)(unsigned action,BOOL *enabled),
    void (*connection_status)(char *text,unsigned capacity));
void SudekiMpLanPartyToolsClose(void);
/* Local input only: never pauses the world or another network seat. */
BOOL SudekiMpLanPartyToolsCaptureInput(void);
BOOL SudekiMpLanPartyToolsRestoreRenderState(void);
BOOL SudekiMpLanPartyToolsRenderDrained(void);
BOOL SudekiMpInstallIntegratedCleanroomMenu(
    HMODULE game_module,
    UINT toggle_key
);
/* LAN host owns controller/render hooks elsewhere. This installs only the
 * host-authoritative cleanroom tools UI and never enables local split roles. */
BOOL SudekiMpInstallLanArenaHostCleanroomMenu(
    HMODULE game_module,
    UINT toggle_key
);
BOOL SudekiMpInstallZoneTraversalMenu(
    HMODULE game_module,
    UINT toggle_key,
    BOOL skip_startup_movies
);
BOOL SudekiMpInstallCoopRosterMenu(
    HMODULE game_module,
    UINT toggle_key,
    BOOL integrated_multiplayer,
    BOOL skip_startup_movies,
    BOOL story_test_boost_enabled,
    UINT story_test_boost_key,
    float story_test_boost_multiplier
);
/* Requests the fixed Tal (host) / Ailish (Player 2) co-op roster only after
 * a loaded save has restored a stable world and party.  This setter merely
 * arms the game-thread service; it never changes party pointers itself. */
void SudekiMpCleanroomMenuSetLoadedSaveCoopAutostart(BOOL enabled);
void SudekiMpCleanroomMenuUpdate(void);
void SudekiMpCleanroomMenuRender(void);
BOOL SudekiMpCleanroomMenuInstalled(void);
BOOL SudekiMpCleanroomMenuActive(void);
/* Read-only bridge for code that already depends on the cleanroom presenter;
 * the save-book interceptor owns exact native opening/closed observation. */
BOOL SudekiMpCleanroomNativeSaveModalActive(void);
void SudekiMpUninstallCleanroomMenu(void);

#endif
