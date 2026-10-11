#ifndef SUDEKIMP_LAN_STORY_SPAWN_TRACE_H
#define SUDEKIMP_LAN_STORY_SPAWN_TRACE_H
#include <windows.h>
/* #54 research, observe only: log native spawn-group calls (CreateSpawnGroup,
 * the three InternalStartSpawnGroupSequence overloads, DespawnEntity) and
 * call each original unchanged. Exact entry bytes; game thread. */
BOOL SudekiMpLanStorySpawnTraceInstall(HMODULE image);
BOOL SudekiMpLanStorySpawnTraceUninstall(void);
#include "network/lan_story_effect_state.h"
/* Host: spawn groups started natively this session (newest sequence each). */
unsigned SudekiMpLanStorySpawnTraceGroups(SudekiMpStorySpawnGroup *out,unsigned max);
/* Client: start the same native spawn group sequence locally (game thread). */
BOOL SudekiMpLanStorySpawnReplay(const char *group,int sequence);
/* Client: run the native update of replayed groups the world pause keeps
 * unscheduled (game thread). TRUE only after entering a native group/effect
 * update; watched, idle or refused groups grant no residency adoption.
 * Client SpecialEffect lifecycles continue independently of group completion. */
BOOL SudekiMpLanStorySpawnTick(uint32_t now,BOOL client);
/* Portable identity of a monster spawned by a group this machine started:
 * hash(group identifier, spawn point index, monster type). Game thread. */
BOOL SudekiMpLanStorySpawnIdentity(const void *monster,uint32_t *identity);
/* Call use() with a native ResourceName built from name (game thread). */
BOOL SudekiMpLanStoryWithResourceName(const char *name,unsigned kind,
    void (*use)(void *resource,void *context),void *context);
/* Host: recent despawned monster identities. Client: follow them once.
 * Follow returns TRUE only if a native despawn was entered. */
void SudekiMpLanStorySpawnDespawnCapture(SudekiMpStoryDespawn *out);
BOOL SudekiMpLanStorySpawnDespawnFollow(const SudekiMpStoryDespawn *host);
/* Read-only host cluster/sigil observations. Unknown observations stay unknown. */
void SudekiMpLanStorySpawnEffectsCapture(SudekiMpStoryEffectState *out);
void SudekiMpLanStorySpawnSigilsFollow(const SudekiMpStorySigilState *host);
#endif
