#ifndef SUDEKIMP_LAN_STORY_SPAWN_TRACE_H
#define SUDEKIMP_LAN_STORY_SPAWN_TRACE_H
#include <windows.h>
/* Saved-story host spawn observation and client occurrence replay. Exact
 * supported-image hooks; native creation/lifetime remain on the game thread. */
BOOL SudekiMpLanStorySpawnTraceInstall(HMODULE image,BOOL client);
BOOL SudekiMpLanStorySpawnTraceUninstall(void);
#include "network/lan_story_effect_state.h"
/* Host: spawn groups started natively this session (newest sequence each). */
unsigned SudekiMpLanStorySpawnTraceGroups(SudekiMpStorySpawnGroup *out,unsigned max);
/* Client: start the same native spawn group sequence locally (game thread). */
BOOL SudekiMpLanStorySpawnReplay(const SudekiMpStorySpawnGroup *command);
/* Client: advance native SpecialEffect lifecycles; never choose spawn points
 * or run group timers. TRUE only after entering a native effect update. */
BOOL SudekiMpLanStorySpawnTick(uint32_t now,BOOL client);
/* Host occurrence serial of a positively witnessed native monster. */
BOOL SudekiMpLanStorySpawnEventsCapture(SudekiMpStoryEffectState *out);
BOOL SudekiMpLanStorySpawnEventsFollow(const SudekiMpStoryEffectState *host);
BOOL SudekiMpLanStorySpawnIdentity(const void *monster,uint32_t *identity);
/* Host death state and observed authored emission; client presents it once
 * per leased occurrence. Native death/loot and final removal stay separate. */
BOOL SudekiMpLanStorySpawnDeathState(const void *monster,BOOL *dead,unsigned *emitted);
BOOL SudekiMpLanStorySpawnDeathPresent(void *monster,uint32_t identity,unsigned style);
/* Call use() with a native ResourceName built from name (game thread). */
BOOL SudekiMpLanStoryWithResourceName(const char *name,unsigned kind,
    void (*use)(void *resource,void *context),void *context);
/* Host: recent death styles, consumed by the complete spawn roster follower. */
void SudekiMpLanStorySpawnDespawnCapture(SudekiMpStoryDespawn *out);
/* Read-only host cluster/sigil observations. Unknown observations stay unknown. */
void SudekiMpLanStorySpawnEffectsCapture(SudekiMpStoryEffectState *out);
void SudekiMpLanStorySpawnSigilsFollow(const SudekiMpStorySigilState *host);
/* Automatic combat gates: authenticated effects carry complete liveness;
 * world poses carry host transforms and animation. Game-thread calls only. */
BOOL SudekiMpLanStoryGatesCapture(SudekiMpStoryEffectState *out);
BOOL SudekiMpLanStoryGatesFollow(const SudekiMpStoryEffectState *host);
BOOL SudekiMpLanStoryGateIdentity(const void *entity,uint32_t *identity);
/* Client, game thread: hide the reveal-marker effects standing at a chest or
 * door the host has fully revealed (alpha 255, #79). Visual flags only; TRUE if any hid. */
BOOL SudekiMpLanStoryRevealMarkersHide(const float position[3]);
#endif
