#ifndef SUDEKIMP_LAN_STORY_HOST_CONTROL_H
#define SUDEKIMP_LAN_STORY_HOST_CONTROL_H

#include "hooks/lan_story_observer.h"

/* Saved-story HOST only. This separate sparse-roster adapter does not relax
 * the four-character/TestRoom control adapter. It owns native None/All input
 * filtering for the custom menu and validated native party rotation for
 * host-approved swaps. The closed Lighthouse journal separately authorizes
 * restoring Tal after recruitment. It never writes actor/controller/group identity fields. */
/* Default-off castle arrival scope, supplied on an exact native dispatch.
 * remote_or_retained keeps host input closed until a disconnected lease is
 * actually drained. A reservation survives temporary host control. */
BOOL SudekiMpLanStoryHostControlLeadPolicy(void *controller,
    const SudekiMpControlUpdateDispatchWitness *,const SudekiMpLanStoryScene *,
    BOOL enabled,BOOL remote_or_retained);
BOOL SudekiMpLanStoryHostControlRemoteLead(const SudekiMpLanStoryNativeRoster *);
BOOL SudekiMpLanStoryHostControlTemporaryLead(const SudekiMpLanStoryNativeRoster *);
BOOL SudekiMpLanStoryHostControlInstall(HMODULE image,unsigned locked_character);
/* Sample the scene with StoryObserver first on this exact post-controller
 * dispatch. FALSE means pending/unknown or a native rotation occurred: do not
 * publish using that pre-call scene. A fresh Sample after the call is allowed
 * only if the same dispatch witness still proves exact; it does not complete
 * the rotation acknowledgment, which requires the next dispatch.
 * Opening the menu retains the same input owner and postpones all rotation. */
BOOL SudekiMpLanStoryHostControlService(void *controller,
    const SudekiMpControlUpdateDispatchWitness *witness,
    const SudekiMpLanStoryScene *scene,BOOL menu_open);
/* Fresh exact bound-character proof for UI/readiness. An owned menu fence
 * can be exact here; this does not grant gameplay input while the menu is
 * open. Positively owned story remote companions may hold ref1/mode0 here;
 * the native rotation path still requires all actor overrides to be zero.
 * Capture/streaming remains independent of a pending input transition. */
BOOL SudekiMpLanStoryHostControlReady(void *controller,
    const SudekiMpControlUpdateDispatchWitness *witness,
    const SudekiMpLanStoryScene *scene);
/* Same exact binding while the front plays a native use animation (chest,
 * breakable, usable) that switched the controller off. Grants remote
 * players their own steering/actions only; never host input. */
BOOL SudekiMpLanStoryHostControlFrontUseLock(void *controller,
    const SudekiMpControlUpdateDispatchWitness *witness,
    const SudekiMpLanStoryScene *scene);
/* Existing companion ownership depends on the exact host actor binding,
 * not on whether a native interaction currently accepts host input. This
 * proves the same roster/AI/controller ownership as Ready, but does not
 * authorize acquisition or movement through a foreign input filter. */
BOOL SudekiMpLanStoryHostControlBound(void *controller,
    const SudekiMpControlUpdateDispatchWitness *witness,
    const SudekiMpLanStoryScene *scene);
/* Existing HELD exterior actor only, after the split owner positively proves
 * a settled TEMP or a completed balanced return and that actor's retained
 * exterior membership. Bound retains its independent lease across the lead's
 * native interaction transition. The task owner's exact host-local interior
 * approach proof also permits retention/steering before TEMP begins, with
 * the same world/descriptor/group/controller/lead and existing remote lease.
 * Ready requires the settled TEMP proof plus
 * ordinary host input or the native shop modal for action admission; a return
 * proof never grants actions.
 * Neither permits acquisition, rotation, or input to the host's interior.
 * Session/ACK, world pause and the remote body's action guards remain callers'
 * responsibility. These never change the host's AI or native input filter. */
BOOL SudekiMpLanStoryHostControlExteriorBound(void *controller,
    const SudekiMpControlUpdateDispatchWitness *,const SudekiMpLanStoryScene *,unsigned character);
BOOL SudekiMpLanStoryHostControlExteriorReady(void *controller,
    const SudekiMpControlUpdateDispatchWitness *,const SudekiMpLanStoryScene *,unsigned character);
/* Between dispatches, revalidate the retained roster on the native thread;
 * a copied controller witness is not authority for a native callback. */
BOOL SudekiMpLanStoryHostControlExteriorNativeBound(const SudekiMpLanStoryNativeRoster *,unsigned character);
BOOL SudekiMpLanStoryHostControlExteriorNativeReady(const SudekiMpLanStoryNativeRoster *,unsigned character);
/* Steering/turn only: same positive exterior, approach or completed-return binding, independently
 * of the lead's native modal filter, and the remote actor's pause count zero.
 * World/body, session/ACK and packet freshness remain mandatory. A paused
 * body must be held without even a neutral native movement submission.
 * This does not replace NativeReady for action submission. */
BOOL SudekiMpLanStoryHostControlExteriorNativeSteeringReady(const SudekiMpLanStoryNativeRoster *,unsigned character);
/* Stop requesting rotations/menu input ownership, then service native filter
 * retirement before removing the story observer or destroying the world.
 * A native rotation already entered must first yield a coherent binding. */
/* Coordinator reserves the target and drains remote leases before requesting
 * native selection. Ready acknowledges the target on a fresh dispatch. */
BOOL SudekiMpLanStoryHostControlCanSwap(void *controller,
    const SudekiMpControlUpdateDispatchWitness *,const SudekiMpLanStoryScene *);
BOOL SudekiMpLanStoryHostControlSelect(void *controller,
    const SudekiMpControlUpdateDispatchWitness *,const SudekiMpLanStoryScene *,unsigned target);
BOOL SudekiMpLanStoryHostControlDrain(void *controller,
    const SudekiMpControlUpdateDispatchWitness *witness,
    const SudekiMpLanStoryScene *scene);
BOOL SudekiMpLanStoryHostControlRetains(void);
BOOL SudekiMpLanStoryHostControlUninstall(void);

#endif
