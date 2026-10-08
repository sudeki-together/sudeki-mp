#ifndef SUDEKIMP_LAN_STORY_ALLY_HUD_H
#define SUDEKIMP_LAN_STORY_ALLY_HUD_H
#include "network/lan_story_handoff.h"
#include <windows.h>
#include <stdint.h>
#include <stddef.h>
/* Native combo reader (COMBO_GIZMO: three input slots that flash on a completed
 * combo). Legacy ally-seat modes retain the behavior below; ConfigureAvatars
 * also supports four independent observed avatars and the host's local HUD.
 *
 * Host ([DevPlay] AllySeatPlayer): observe-only inline hooks on the three
 * CComboManager -> HUD seams (accepted input 0x4d14d0, chain reset 0x4d0440,
 * combo complete 0x4d0a10). The native code feeds the HUD only when the
 * manager's owner is the controlled hero; this module keeps the slot state the
 * HUD would show if the owner were the ally, and publishes it to the ally seat.
 * The observer never writes into combo managers. Legacy host mode does not
 * change the host's HUD; avatar mode presents an explicitly supplied snapshot.
 *
 * Client ([DevPlay] ClientAlly): wraps the HUD controller update (0x4ae430).
 * While the local seat drives the ally and the host reports melee combat, the
 * HUD is held in melee mode 3 and the host's slot state is written into the
 * controller's slot words, then refreshed through the native element update.
 * Otherwise the native update runs unchanged. */
/* Loader composition: enable the new per-avatar observer and local display
 * capabilities before Install. Legacy INI switches keep their old meaning. */
BOOL SudekiMpLanStoryAllyHudConfigureAvatars(BOOL enabled);
BOOL SudekiMpLanStoryAllyHudInstall(HMODULE game_module);
BOOL SudekiMpLanStoryAllyHudUninstall(void);
BOOL SudekiMpLanStoryAllyHudInstalled(void);
/* TRUE when this module's client hook owns the HUD update entry at target and
 * the bytes it replaced equal expected (length bytes): other callers may then
 * treat the hooked entry as the exact native entry and call through it. */
BOOL SudekiMpLanStoryAllyHudOwnsUpdateEntry(const uint8_t *target,const uint8_t *expected,size_t length);
/* Host game thread: the ally entity whose combo manager is mirrored (NULL = none). */
void SudekiMpLanStoryAllyHudTrack(void *entity);
/* Host game thread: current shadow slots plus the combat bit read from the
 * ally's arbiter exactly as the native HUD update reads the hero's. */
BOOL SudekiMpLanStoryAllyHudSnapshot(const void *entity,const SudekiMpLanStoryControlFence *fence,
    SudekiMpLanStoryAllyHud *hud);
/* Four independent host shadows. Caller supplies a stable callback/context
 * lasting until this track is cleared or successful uninstall. It is invoked
 * synchronously on every matching native event and Snapshot, and must freshly
 * prove player/entity/spawn-generation plus current world/control ownership;
 * it must not mutate/reenter this module. Losing that proof clears only this
 * player's track and shadow. The local host is player 0; wire fences still
 * remain client-only. No actor or component is selected/changed here. */
typedef BOOL (*SudekiMpLanStoryAllyHudAvatarExact)(unsigned player,const void *entity,
    uint32_t spawn_generation,void *context);
BOOL SudekiMpLanStoryAllyHudTrackAvatar(unsigned player,void *entity,uint32_t spawn_generation,
    SudekiMpLanStoryAllyHudAvatarExact exact,void *context);
BOOL SudekiMpLanStoryAllyHudSnapshotAvatar(unsigned player,const void *entity,uint32_t spawn_generation,
    const SudekiMpLanStoryControlFence *fence,SudekiMpLanStoryAllyHud *hud);
/* Game thread: present latest host record (NULL = release), including player 0
 * for the host's local avatar when ConfigureAvatars enabled the update hook.
 * Client records retain their HOST fence generation, not the local mirror's
 * spawn generation. Caller owns freshness and current local fence admission. */
void SudekiMpLanStoryAllyHudClientPresent(const SudekiMpLanStoryAllyHud *hud);
#endif
