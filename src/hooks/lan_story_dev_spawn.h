#ifndef SUDEKIMP_LAN_STORY_DEV_SPAWN_H
#define SUDEKIMP_LAN_STORY_DEV_SPAWN_H
#include "hooks/lan_story_observer.h"
#include <windows.h>
/* Dev Play ([DevPlay] SpawnKey, SpawnResource, SpawnOffset; saved-story host):
 * a key press spawns one generic entity beside the party leader through the
 * cleanroom engine's native spawn export. Native-owned lifetime; bounded. */
void SudekiMpLanStoryDevSpawnConfigure(const wchar_t *config_path);
void SudekiMpLanStoryDevSpawnService(const SudekiMpLanStoryNativeRoster *roster,HMODULE game_module);
/* Client mirror ([DevPlay] ClientSpawnMirror=<NAME>, the host's SpawnResource):
 * the paused client owns one hidden copy of that monster so the world stream
 * can present the host's spawn. Like the ally mirror it is spawned before the
 * native pause is acquired (may_spawn), never under it; the host owns its AI,
 * health and damage. The world adapter shows it only while the host has one. */
void SudekiMpLanStoryDevSpawnClientService(const SudekiMpLanStoryNativeRoster *roster,BOOL may_spawn,HMODULE game_module);
/* TRUE when the client may acquire the pause: no mirror configured, the copy
 * is resolved and hidden, or the spawn wait timed out (logged). */
BOOL SudekiMpLanStoryDevSpawnClientReady(void);
#endif
