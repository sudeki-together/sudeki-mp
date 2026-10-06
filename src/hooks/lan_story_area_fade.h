#ifndef SUDEKIMP_LAN_STORY_AREA_FADE_H
#define SUDEKIMP_LAN_STORY_AREA_FADE_H
#include "hooks/lan_story_replica.h"

/* Client-only cosmetic fade for party characters in a different host area
 * (inside a house/church while this player stays outside). The native zone
 * fade writes a per-object alpha into the top byte of the scene object's
 * colour dword (object+0x10) and skips drawing at zero; this driver ramps that
 * byte for the character's world scene object (entity +0x44 CPosition render
 * wrapper +0xB4, object +8) over ~0.6 s, raises the native hidden flag (+0x34
 * bit 0x04, with the renderer visibility callback when bit 0x4000000 is set)
 * once fully faded, and reverses on return. Original colour/flags are restored
 * when the character leaves the roster, its object changes, or on Reset.
 * No pose, resource or gameplay state is touched; host processes never use it. */
void SudekiMpLanStoryAreaFadeEnable(BOOL enabled);
/* Runtime present path (client): characters currently in another host area. */
void SudekiMpLanStoryAreaFadeSetForeign(uint8_t mask);
/* Inside the ClientPresent callback after the world pose apply, with the
 * proven roster. Reads/writes only the colour dword and hidden flag. */
void SudekiMpLanStoryAreaFadeApply(const SudekiMpLanStoryNativeRoster *roster);
/* 0..1 presentation alpha for name tags and similar overlays. */
float SudekiMpLanStoryAreaFadeAlpha(unsigned character);
/* Restore every touched object that still resolves; runtime retire / world change. */
void SudekiMpLanStoryAreaFadeReset(void);
#endif
