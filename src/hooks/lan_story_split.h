#ifndef SUDEKIMP_LAN_STORY_SPLIT_H
#define SUDEKIMP_LAN_STORY_SPLIT_H
#include <windows.h>
#include <stdint.h>

/* Host-only split-area owner for saved-story (first milestone: the host's
 * lead uses an authored TEMP door while other party characters stay in the
 * exterior). Joins three existing pieces under one policy:
 *  - story observer split callbacks (exterior epoch retained, lead inside);
 *  - lan_story_temp_exterior keep-live consumer (no whole-exterior freeze);
 *  - CGroupPlayers::SetModeLeadOnly/SetModeFullParty entry wrappers that
 *    exempt the outside characters from the script's follower suspension.
 *    The exemption removes exactly the one reference native lead-only added
 *    (native single-node resume) and restores it immediately before the
 *    matching full-party release (native single-node pause), so native
 *    counters stay balanced.
 * Not a background loader, client door path or collision filter. Remote
 * players cannot initiate doors; the host must be the one entering. */
BOOL SudekiMpLanStorySplitInstall(HMODULE);
/* Refused while a split, exemption or kept exterior is outstanding. */
BOOL SudekiMpLanStorySplitUninstall(void);
/* Host runtime, once per native dispatch. outside_mask: characters that may
 * stay outside (remote-controlled, currently held by their players). */
void SudekiMpLanStorySplitSetEligibility(BOOL host_ready,uint8_t outside_mask);
BOOL SudekiMpLanStorySplitActive(void);
/* Host world capture: native zone data whose authored spawns belong to the
 * occupied TEMP while a split is active, else NULL. Copy-only identity. */
const void *SudekiMpLanStorySplitTemporaryData(void);
#endif
