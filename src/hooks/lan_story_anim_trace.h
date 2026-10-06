#ifndef SUDEKIMP_LAN_STORY_ANIM_TRACE_H
#define SUDEKIMP_LAN_STORY_ANIM_TRACE_H
#include <windows.h>

/* Probe-only, read-only observer for the animation renderer update seam
 * (cAnimObjectRenderer vtable 0x6DF8EC slot 2, RVA 0x222B50: Update(ctx, dt)).
 * One log-only entry hook that copies the caller, the dt argument and the
 * renderer identity, then runs the original unchanged. Calls are aggregated
 * per caller RVA and summarised every two seconds (bounded line budget):
 * total calls, calls with a non-zero dt and distinct renderers seen with and
 * without a non-zero dt. Purpose: find the native per-frame caller and its
 * visibility gate for off-camera actors. Not a pause owner, animation driver
 * or gameplay change. Installed only by the [StoryAreas] AnimTrace profile. */
BOOL SudekiMpLanStoryAnimTraceInstall(HMODULE);
BOOL SudekiMpLanStoryAnimTraceUninstall(void);
/* Optional watched renderer (copy of a pointer; never dereferenced). The
 * window summary reports its calls, non-zero dt calls and caller RVAs. */
void SudekiMpLanStoryAnimTraceWatch(const void *renderer);
#endif
