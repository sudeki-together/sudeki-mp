#ifndef SUDEKIMP_LAN_PARTY_CAST_H
#define SUDEKIMP_LAN_PARTY_CAST_H
#include "hooks/lan_party_control.h"
#include "hooks/lan_arena_spirit_visual_host.h"

/* Four native actor namespaces, independent of immutable network seat order.
 * Only the host starts gameplay. A client needs an exact admitted frame for
 * an ordinary CSkill replay; Spirit managers are never scheduled on replicas. */
BOOL SudekiMpLanPartyCastInstall(HMODULE image,SudekiMpLanPartySession *session);
/* Exact patch-owner witness for adapters sharing the SMP4 normal-time policy. */
BOOL SudekiMpLanPartyCastRealtimeExact(HMODULE image);
BOOL SudekiMpLanPartyCastService(const SudekiMpControlUpdateDispatchWitness *w);
/* Startup only, after the coordinator reports all four actors initialized.
 * Service continues existing native lifetimes even while that gate is closed. */
BOOL SudekiMpLanPartyCastTryBind(const SudekiMpControlUpdateDispatchWitness *w);
/* Local controller switching is a separate native transaction. Probe before
 * it, then commit only after the native controller/roster owner and session
 * assignment agree on the new canonical character. These retain every caster
 * namespace and callback; they neither switch a controller nor release leases. */
BOOL SudekiMpLanPartyCastCanRebindLocal(const SudekiMpControlUpdateDispatchWitness *w);
/* Native service thread only. Conservative all-actor namespace closure for
 * transfers; no menu fence required. Does not gate ordinary body playback. */
BOOL SudekiMpLanPartyCastTransferReady(void);
/* Read-only native lifetime witness for an already admitted actor emission;
 * grants no ability/shot authority and survives transport lease replacement. */
BOOL SudekiMpLanPartyCastRetainedEffectOwner(void *actor,SudekiMpLanPartyEffectOwner *owner);
BOOL SudekiMpLanPartyCastRebindLocal(const SudekiMpControlUpdateDispatchWitness *w,
    unsigned character);
/* Shutdown-only: closes cast admission and commits the already completed
 * native physical switch even if the transport assignment has disappeared.
 * Requires the same canonical actors, idle descendants, and owned UI fence. */
BOOL SudekiMpLanPartyCastRebindLocalForCleanup(const SudekiMpControlUpdateDispatchWitness *w,
    unsigned character);
/* Finish an already coherent native switch while this client's transport is
 * FREE/DRAINING. Keeps namespaces available for rejoin; no input admission. */
BOOL SudekiMpLanPartyCastRebindLocalDisconnected(const SudekiMpControlUpdateDispatchWitness *w,
    unsigned character);
BOOL SudekiMpLanPartyCastReady(void);
BOOL SudekiMpLanPartyCastActive(void *actor);
/* Positive, generation-bound native targeting observation. Unknown, release,
 * Spirit and disconnected owners cannot admit aim-only input. */
BOOL SudekiMpLanPartyCastTargeting(void *actor);
BOOL SudekiMpLanPartyCastLocalTargeting(void);
BOOL SudekiMpLanPartyCastNoncaster(void *actor);
BOOL SudekiMpLanPartyCastLocalNoncaster(void *actor);
BOOL SudekiMpLanPartyCastDrained(void *actor);
/* Renderer/capture handoff only. Lighting still retains its full lifetime;
 * actor release, new cast admission and teardown use CastDrained. */
BOOL SudekiMpLanPartyCastBodyIdle(void *actor);
BOOL SudekiMpLanPartyCastSubmit(const SudekiMpControlUpdateDispatchWitness *w,
    const SudekiMpLanPartyLease *lease,void *actor,BOOL spirit,unsigned slot);
BOOL SudekiMpLanPartyCastCapture(unsigned seat,SudekiMpLanArenaActorSnapshot *snapshot);
/* Consume authenticated lifecycle data before presentation handoff. This
 * starts no task and never cancels one; native scripts still retire normally. */
BOOL SudekiMpLanPartyCastReplayTiming(const SudekiMpControlUpdateDispatchWitness *w,
    const SudekiMpLanPartyLease *lease,const SudekiMpLanPartyFrame *frame);
BOOL SudekiMpLanPartyCastReplay(const SudekiMpControlUpdateDispatchWitness *w,
    const SudekiMpLanPartyLease *lease,const SudekiMpLanPartyFrame *frame);
BOOL SudekiMpLanPartyCastNativeBody(void *actor);
BOOL SudekiMpLanPartyCastCallbacksRetained(void);
BOOL SudekiMpLanPartyCastLocalCameraActive(void);
int SudekiMpLanPartyCastRouteCamera(void *manager,const char *name);
BOOL SudekiMpLanPartyCastLight(float rgb[3]);
void SudekiMpLanPartyCastRequestStop(void);
BOOL SudekiMpLanPartyCastUninstall(void);
#endif
