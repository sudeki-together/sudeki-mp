#ifndef SUDEKIMP_LAN_STORY_ZONE_BANNER_H
#define SUDEKIMP_LAN_STORY_ZONE_BANNER_H
#include <windows.h>
#include <stdint.h>
#include "hooks/control_separation.h"
/* H1b: the interior zone-name banner the door script shows after entering a
 * house (EnterInterior|SRSBS -> DisplayInteriorZoneName: DisplayMessage(id,0),
 * a 3.0 s wait, HideMessageBox; CONFIRMED_STATIC, SOLWORLDM.gex). A client
 * entering a house locally runs no door script. Sole owner of these native
 * message-box calls; no hooks. Client, native thread only. */
BOOL SudekiMpLanStoryZoneBannerInitialize(HMODULE image);
/* Show string `text_id` (the zone's banner text) once, soon. */
void SudekiMpLanStoryZoneBannerRequest(uint16_t text_id,uint32_t now);
/* Drop a request that has not been shown yet (e.g. the player left). */
void SudekiMpLanStoryZoneBannerCancel(void);
/* Every client frame: shows a pending request when every native gate
 * holds (dropped after 1 s of refusals), hides our own box after 3 s. */
void SudekiMpLanStoryZoneBannerService(uint32_t now,const SudekiMpControlUpdateDispatchWitness *witness);
/* H1b examinable on this client's own game (owner D4/D11): shown in the
 * next Service (inside a residency step): the settled shop's emitter is
 * enabled (when `zone` is still current) and MessageBoxDisplay(text, mode,
 * 0.0, flags) runs, as the examinable's OnAction does natively. */
BOOL SudekiMpLanStoryZoneBannerExamine(const char *zone,uint16_t text,unsigned mode,uint32_t flags,const char *emitter,uint32_t now);
/* Pending or shown (the box owns the confirm press until it closes). */
BOOL SudekiMpLanStoryZoneBannerExamineActive(void);
/* The interact press (TRUE) or its release (FALSE), to our box only. */
void SudekiMpLanStoryZoneBannerExamineInput(BOOL press);
/* Close our box now (Escape, area change, teardown), token-checked. */
void SudekiMpLanStoryZoneBannerExamineClose(const char *why);
/* Runtime teardown, native thread: hide our own box when provable; else the
 * native box timer closes it. Always ends this module's state. */
void SudekiMpLanStoryZoneBannerReset(void);
#endif
