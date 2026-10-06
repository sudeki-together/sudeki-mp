#ifndef SUDEKIMP_LAN_STORY_NAME_TAGS_H
#define SUDEKIMP_LAN_STORY_NAME_TAGS_H
#include <windows.h>

/* Player names in saved-story multiplayer (host and clients):
 *  - HUD: the portrait gizmo resolves the displayed character name through
 *    CALL 0x52A9B0 at RVA 0xA9EB5 (EAX=name table, ECX=portrait enum, caller
 *    EBP=UIPortraitGizmo whose +0x32C is the party slot). This adapter owns
 *    that one relative call and returns the lobby player's name (UTF-16 copy)
 *    for a party slot whose character is held by a named player; every other
 *    case runs the native lookup unchanged.
 *  - Floating tags: for each character held by a named player, the character's
 *    native CPosition translation plus a head offset is projected through the
 *    current render camera and drawn with the existing text-only overlay
 *    (SudekiMpTitleViewDraw overlay mode, 960x720 canvas), from the runtime's
 *    per-frame presentation point. Read-only native access; no UI state write. */
BOOL SudekiMpLanStoryNameTagsInstall(HMODULE);
BOOL SudekiMpLanStoryNameTagsUninstall(void);
void SudekiMpLanStoryNameTagsRender(void *device);
#endif
