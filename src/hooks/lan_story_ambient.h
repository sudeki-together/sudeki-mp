#ifndef SUDEKIMP_LAN_STORY_AMBIENT_H
#define SUDEKIMP_LAN_STORY_AMBIENT_H
#include <windows.h>

/* Client-side ambient animation (saved story). The contained client keeps the
 * native world fully paused, so the frame dispatcher hands every scene walk a
 * zero delta and zone-placed animated props (New Brightwater's river, sea,
 * waterfall, fountain, chimes, leaves, ...) never advance: they are scene
 * objects without an entity, so no replicated pose reaches them either.
 * This adapter owns the cAnimObjectRenderer::Update entry (RVA 0x222B50) and,
 * only while the client runtime renews it each presented frame, replaces a
 * zero incoming delta with a bounded local render delta for renderers no
 * registry entity owns. Entity-owned renderers (replicated NPCs, scenery,
 * party characters, monsters) keep the native zero delta: their poses are
 * written by presentation. Host processes never activate it. Mutually
 * exclusive with the AnimTrace probe, which uses the same entry bytes. */
BOOL SudekiMpLanStoryAmbientInstall(HMODULE image);
BOOL SudekiMpLanStoryAmbientUninstall(void);
/* Runtime, client presentation path only: TRUE renews the activity lease
 * (expires after 500 ms without renewal); FALSE retires it at once. */
void SudekiMpLanStoryAmbientSetActive(BOOL active);
#endif
