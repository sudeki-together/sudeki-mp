#ifndef SUDEKIMP_LAN_STORY_SKY_H
#define SUDEKIMP_LAN_STORY_SKY_H
#include <windows.h>
#include "network/lan_story_effect_state.h"
/* Host sky/fog script calls (#54: the ambush darkens the sky through native
 * SetSkyDomeColour/BlendSkyDomeColour/... from its triggers). The host
 * records the latest call per kind; a client replays newer ones natively.
 * Exact entry bytes; game thread only. EnableFog (a bare JMP) is not hooked. */
BOOL SudekiMpLanStorySkyInstall(HMODULE image);
BOOL SudekiMpLanStorySkyUninstall(void);
/* Game thread: restore the target/rate before an area switch or session exit.
 * FALSE retains the lease for retry; an unknown owner must not be written. */
BOOL SudekiMpLanStorySkyReleaseLighting(void);
/* Host, native thread (H1b): while held, the scene light calls (ambient,
 * light group set/inc, brightness, colour adjust, background) are queued,
 * not applied or published. Release applies them through the exports (and
 * so publishes them) or drops them. Capture ends by itself 35 s after it
 * began (later calls pass through; queued ones wait for Release). */
void SudekiMpLanStorySkyHoldLighting(BOOL hold);
void SudekiMpLanStorySkyReleaseHeld(BOOL apply);
/* Host: copy the latest calls into a presentation report. */
void SudekiMpLanStorySkyCapture(SudekiMpStoryEffectState *out);
/* Client: replay calls newer than the last applied ones. */
void SudekiMpLanStorySkyFollow(const SudekiMpStoryEffectState *host);
void SudekiMpLanStorySkySetClient(BOOL client);
#endif
