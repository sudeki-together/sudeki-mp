#ifndef SUDEKIMP_LAN_STORY_CAST_H
#define SUDEKIMP_LAN_STORY_CAST_H
#include "hooks/lan_story_control.h"

/* Saved-story HOST only. Native namespaces are constructed for the sparse
 * observed party, never for missing characters. Reuses the existing exact
 * skill/task/camera/UI ABI; no client task execution or wire schema change. */
BOOL SudekiMpLanStoryCastInstall(HMODULE image);
/* Construction is explicit, only after the runtime has settled its sparse
 * party and acquired remote control. Service alone never creates owners. */
BOOL SudekiMpLanStoryCastTryBind(const SudekiMpControlUpdateDispatchWitness *w,
    const SudekiMpLanStoryNativeRoster *roster);
BOOL SudekiMpLanStoryCastReady(void);
/* Exact retained host noncaster only. Excludes all native UI/modal screens;
 * used by the existing controller-local skill-input isolation seam. */
BOOL SudekiMpLanStoryCastLocalNoncaster(void *actor);
BOOL SudekiMpLanStoryCastService(const SudekiMpControlUpdateDispatchWitness *w,
    const SudekiMpLanStoryNativeRoster *roster);
/* Caller has already consumed one authenticated request. This rechecks the
 * native control lease immediately before admission and never retries Use. */
unsigned SudekiMpLanStoryCastSubmit(const SudekiMpControlUpdateDispatchWitness *w,
    const SudekiMpLanStoryNativeRoster *roster,const SudekiMpLanPartyLease *key,unsigned slot);
BOOL SudekiMpLanStoryCastDrained(void *actor);
BOOL SudekiMpLanStoryCastRetains(void);
void SudekiMpLanStoryCastRequestStop(void);
BOOL SudekiMpLanStoryCastUninstall(void);
#endif
