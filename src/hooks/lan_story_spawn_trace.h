#ifndef SUDEKIMP_LAN_STORY_SPAWN_TRACE_H
#define SUDEKIMP_LAN_STORY_SPAWN_TRACE_H
#include <windows.h>
/* #54 research, observe only: log native spawn-group calls (CreateSpawnGroup,
 * the three InternalStartSpawnGroupSequence overloads, DespawnEntity) and
 * call each original unchanged. Exact entry bytes; game thread. */
BOOL SudekiMpLanStorySpawnTraceInstall(HMODULE image);
BOOL SudekiMpLanStorySpawnTraceUninstall(void);
#endif
