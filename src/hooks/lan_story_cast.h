#ifndef SUDEKIMP_LAN_STORY_CAST_H
#define SUDEKIMP_LAN_STORY_CAST_H
#include "hooks/lan_arena_spirit_visual_host.h"
#include "hooks/lan_story_control.h"
#include "network/lan_story_vfx.h"
#include "network/lan_story_cast_view.h"
BOOL SudekiMpLanStoryCastShotOwner(void *actor,SudekiMpLanPartyEffectOwner *owner);
/* Only a witnessed native spiritstrike_start emission, never camera/phase inference. */
void SudekiMpLanStoryCastCaptureAudioStart(unsigned character,uint16_t *sequence,uint32_t *tick);
BOOL SudekiMpLanStoryCastCaptureVisuals(SudekiMpStoryVfx *out);
/* One remote character's retained native camera; host-local selection stays native. */
BOOL SudekiMpLanStoryCastCaptureView(unsigned character,SudekiMpStoryCastView *out);

/* Saved-story HOST only. Native namespaces are constructed for the sparse
 * observed party, never for missing characters. Reuses the existing exact
 * skill/task/camera/UI ABI; the client receives cosmetic state only. */
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
/* Same consumed-request fence, one-based native Spirit variant. Shared SSP
 * and gameplay remain native on the host; no contained-client activation. */
unsigned SudekiMpLanStoryCastSubmitSpirit(const SudekiMpControlUpdateDispatchWitness *w,
    const SudekiMpLanStoryNativeRoster *roster,const SudekiMpLanPartyLease *key,unsigned variant);
BOOL SudekiMpLanStoryCastDrained(void *actor);
/* Movement/basic melee may resume after positive native body/input/task drain.
 * A lighting tail retains its namespace; it still blocks new casts, transfer
 * and teardown through the full CastDrained contract above. */
BOOL SudekiMpLanStoryCastBodyDrained(void *actor);
/* Positive host skill targeting window only; never substitutes for drain. */
BOOL SudekiMpLanStoryCastAiming(void *actor);
BOOL SudekiMpLanStoryCastRetains(void);
void SudekiMpLanStoryCastRequestStop(void);
BOOL SudekiMpLanStoryCastUninstall(void);
#endif
