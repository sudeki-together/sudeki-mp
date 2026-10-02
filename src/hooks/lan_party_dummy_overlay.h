#ifndef SUDEKIMP_LAN_PARTY_DUMMY_OVERLAY_H
#define SUDEKIMP_LAN_PARTY_DUMMY_OVERLAY_H
#include <windows.h>

/* No hooks or actor writes. Render requires the client's current validated
 * presentation lease and the existing phase-2 render-thread boundary. */
BOOL SudekiMpLanPartyDummyOverlayReset(void);
void SudekiMpLanPartyDummyOverlayToggle(void);
BOOL SudekiMpLanPartyDummyOverlayEnabled(void);
void SudekiMpLanPartyDummyOverlayRequestStop(void);
BOOL SudekiMpLanPartyDummyOverlayDrained(void);
BOOL SudekiMpLanPartyDummyOverlayRestore(void);
void SudekiMpLanPartyDummyOverlayRender(HMODULE module,unsigned seat);
#endif
