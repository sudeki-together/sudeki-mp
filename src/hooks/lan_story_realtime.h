#ifndef SUDEKIMP_LAN_STORY_REALTIME_H
#define SUDEKIMP_LAN_STORY_REALTIME_H
#include <windows.h>

/* Saved-story host AND clients, installed before native saved-world load.
 * Keep every game-speed source at unity while preserving native mode changes
 * and all full-pause/reference operations. Mutually exclusive with SMP4's
 * cast-owned policy: exact pristine seams are required, never borrowed.
 * Installation/removal require the runtime's quiescent native UI boundary. */
BOOL SudekiMpLanStoryRealtimeInstall(HMODULE image);
BOOL SudekiMpLanStoryRealtimeExact(HMODULE image);
BOOL SudekiMpLanStoryRealtimeUninstall(void);
#endif
