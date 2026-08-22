#ifndef SUDEKIMP_ZONE_TRANSITION_TRACE_H
#define SUDEKIMP_ZONE_TRANSITION_TRACE_H

#include <windows.h>

/* Observation-only door/zone transition tracing for the supported GOG build. */
BOOL SudekiMpInstallZoneTransitionTrace(HMODULE game_module);
void SudekiMpUninstallZoneTransitionTrace(void);

/* World-aware traversal actions. These are available only after the exact
 * transition trace has been installed and are intended for the developer
 * traversal menu, never for ordinary gameplay. */
BOOL SudekiMpZoneTraversalSwitchWorld(const char *zone_name);
BOOL SudekiMpZoneTraversalEnterTemporary(const char *zone_name);
BOOL SudekiMpZoneTraversalExitTemporary(void);
const char *SudekiMpZoneTraversalCurrentWorld(void);
const char *SudekiMpZoneTraversalCurrentTemporary(void);
BOOL SudekiMpZoneTraversalWorldMatches(const char *zone_name);
/* True when the destination is present in the developer traversal registry
 * and may be auto-discovered through Sudeki's native transition pipeline. */
BOOL SudekiMpZoneTraversalKnownDestination(
    const char *world_name,
    const char *temporary_name
);
/* A destination is safe to revisit only after a native save/door transition
 * has supplied an authored arrival anchor for it.  Known destinations may be
 * launched once without a cache; the transition trace captures their native
 * placement automatically and promotes the result to the cache. */
BOOL SudekiMpZoneTraversalArrivalContextReady(
    const char *world_name,
    const char *temporary_name
);
/* Apply the cached native arrival anchor to currently-present cleanroom PCs
 * after the destination load has settled. */
BOOL SudekiMpZoneTraversalApplyArrivalContext(void);
void SudekiMpZoneTraversalService(void);

#endif
