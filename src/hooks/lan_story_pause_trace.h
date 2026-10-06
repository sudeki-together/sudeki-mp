#ifndef SUDEKIMP_LAN_STORY_PAUSE_TRACE_H
#define SUDEKIMP_LAN_STORY_PAUSE_TRACE_H
#include <windows.h>

/* Probe-only, read-only observer for the TEMP exterior research profile.
 * Entry hooks on native pause sources that change entity pause bytes outside
 * TEMP whole-area suspension: Pause/UnpauseSpawnGroup, GELGroupPtr::Push/
 * PopPaused, the PauseEverything worker, CGameSpeed::SetGamePaused and both
 * CClusterManager::ActivateCluster overloads, plus native NPC cluster activity
 * pause/resume (0x53D540/0x53D5B0, EAX=spawn row). Each call logs its caller and
 * copied arguments (bounded line budget), then runs the original unchanged.
 * Not a pause owner, filter or gameplay change. Not installed by story runtime. */
BOOL SudekiMpLanStoryPauseTraceInstall(HMODULE);
BOOL SudekiMpLanStoryPauseTraceUninstall(void);
#endif
