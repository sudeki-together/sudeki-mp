#ifndef SUDEKIMP_LAN_STORY_ALLY_SEAT_H
#define SUDEKIMP_LAN_STORY_ALLY_SEAT_H
#include "hooks/lan_story_observer.h"
#include <windows.h>
#include <stdint.h>
/* Dev Play ally seat (host only). One remote player drives a host-spawned
 * non-hero entity (default ALLY_TALOS) through the story control path as seat
 * SUDEKIMP_STORY_CHARACTER_ALLY instead of a hero. The entity never joins the
 * native party group; its AI is overridden exactly like a hero companion's.
 * Config: [DevPlay] AllySeatPlayer=1..3 (0/absent = off), AllyResource=NAME. */
void SudekiMpLanStoryAllySeatConfigure(HMODULE game_module,const wchar_t *config_path);
unsigned SudekiMpLanStoryAllySeatPlayer(void);
const char *SudekiMpLanStoryAllySeatResource(void);
/* Host game thread, once per service tick with an exact roster: spawns once
 * per observer epoch next to the leader, then re-resolves and proves the
 * entity. Publishes it to the control layer; withdraws it when lost. */
void SudekiMpLanStoryAllySeatService(const SudekiMpLanStoryNativeRoster *roster,BOOL world_ready);
/* Client mirror ([DevPlay] ClientAlly=true): the paused client owns a matching
 * ALLY_TALOS so the world catalogues agree. It is spawned before the native
 * pause is acquired (may_spawn), never under it, and never AI-overridden. */
BOOL SudekiMpLanStoryAllySeatClientEnabled(void);
void SudekiMpLanStoryAllySeatClientService(const SudekiMpLanStoryNativeRoster *roster,BOOL may_spawn);
/* TRUE when the client may acquire the pause: no client ally configured, the
 * ally is resolved and settled, or the spawn wait timed out (logged). */
BOOL SudekiMpLanStoryAllySeatClientReady(void);
/* The proved entity and its generation (bumps on every new entity). */
BOOL SudekiMpLanStoryAllySeatReady(void **entity,uint32_t *generation);
void SudekiMpLanStoryAllySeatReset(void);
/* Lobby-owned Dev Play path, separate from the legacy research INI route.
 * Only Talos (5) spawns. Native heroes and duplicate-hero spectators do not.
 * Configure plain choices before a world loads; native ownership is granted
 * only by the shared EntitySetup completion/destruction observer. */
BOOL SudekiMpLanStoryAvatarSeatsConfigure(HMODULE image,const uint8_t avatars[4]);
BOOL SudekiMpLanStoryAvatarSeatsEnabled(void);
BOOL SudekiMpLanStoryAvatarSeatChosen(unsigned player);
/* Caller owns a freshly exact roster at a verified game-thread seam.
 * Clients call before acquiring their full-world pause. Each seat has one
 * request per world; an uncertain native task is never retried or cancelled. */
void SudekiMpLanStoryAvatarSeatsService(const SudekiMpLanStoryNativeRoster *roster,
    BOOL world_ready,BOOL may_spawn);
BOOL SudekiMpLanStoryAvatarSeatReady(unsigned player,
    const SudekiMpLanStoryNativeRoster *roster,void **entity,uint32_t *generation);
/* Read-only HP, max HP, SP, max SP from the exact avatar stats component.
 * Caller supplies the fresh game-thread roster; generation is rechecked after
 * copying. This grants no native control or HUD-slot ownership. */
BOOL SudekiMpLanStoryAvatarSeatStats(unsigned player,
    const SudekiMpLanStoryNativeRoster *roster,uint32_t *generation,float values[4]);
/* Dev Play native-hero status uses the same component/value checks, bounded
 * by two fresh native roster observations. Caller owns the capture generation;
 * this is a read-only presentation source, never permission to write stats. */
BOOL SudekiMpLanStoryPartySeatStats(unsigned character,
    const SudekiMpLanStoryNativeRoster *roster,float values[4]);
/* Every configured copy must be ready. Timeout is not permission to pause. */
BOOL SudekiMpLanStoryAvatarSeatsReady(const SudekiMpLanStoryNativeRoster *roster);
/* Call only after the shared task owner positively witnesses native Quit.
 * Refuses to forget in-flight or live native records. */
BOOL SudekiMpLanStoryAvatarSeatsShutdown(void);
#endif
