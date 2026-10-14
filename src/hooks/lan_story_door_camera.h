#ifndef SUDEKIMP_LAN_STORY_DOOR_CAMERA_H
#define SUDEKIMP_LAN_STORY_DOOR_CAMERA_H
#include <windows.h>
#include <stdint.h>
#include "hooks/lan_story_observer.h"
#include "network/lan_story_cast_view.h"

/* Door camera for remote characters (#67 H1b), host side. The native lead
 * path (CDoor traverse helper RVA 0xCEA30) moves the player camera to one of
 * the door's two authored static cameras while the lead walks through. The
 * client's camera is a mod-computed view, so the host computes that same
 * view for a remote character standing in an open door's traverse zone and
 * publishes it through the existing cast-view channel as kind 3; kind 0
 * releases it. Native reads only, plus the game's own static-camera resource
 * lookup (RVA 0x3E540, the call SetStaticCameraPosition itself makes) with a
 * NULL reference pointer; no camera, door or world state is written. */
BOOL SudekiMpLanStoryDoorCameraInstall(HMODULE image);
/* Host game thread, once per published cast view with kind 0. `roster` is a
 * freshly observed native roster; `character` must be available and not the
 * leader. On an engaged door the view is rewritten to kind 3 and TRUE is
 * returned; FALSE leaves the kind-0 view untouched (release or no door). */
BOOL SudekiMpLanStoryDoorCameraView(const SudekiMpLanStoryNativeRoster *roster,unsigned character,
    uint32_t now,SudekiMpStoryCastView *view);
void SudekiMpLanStoryDoorCameraUninstall(void);
#endif
