#ifndef SUDEKIMP_LAN_STORY_TALOS_DAMAGE_PROBE_H
#define SUDEKIMP_LAN_STORY_TALOS_DAMAGE_PROBE_H

#include <windows.h>

/* Observe-only research adapter. Install at the loader's quiescent startup
 * boundary before any damage-entry owner installs, after file SHA validation.
 * Owns six direct CALL operands, never either damage entry. Later native
 * damage containment/feedback entry owners remain in the call chain.
 * Uninstall at a quiescent boundary; explicit DLL unload remains unsupported. */
BOOL SudekiMpLanStoryTalosDamageProbeImageMatches(HMODULE image);
BOOL SudekiMpLanStoryTalosDamageProbeInstall(HMODULE image);
BOOL SudekiMpLanStoryTalosDamageProbeUninstall(void);

#endif
