#ifndef SUDEKIMP_LAN_STORY_HOST_CONTROL_H
#define SUDEKIMP_LAN_STORY_HOST_CONTROL_H

#include "hooks/lan_story_observer.h"

/* Saved-story HOST only. This separate sparse-roster adapter does not relax
 * the four-character/TestRoom control adapter. It owns native None/All input
 * filtering for the custom menu and validated native party rotation for
 * host-approved swaps. The closed Lighthouse journal separately authorizes
 * restoring Tal after recruitment. It never writes actor/controller/group identity fields. */
BOOL SudekiMpLanStoryHostControlInstall(HMODULE image,unsigned locked_character);
/* Separate Dev Play owner may positively prove the saved leader's temporary
 * AI lease and persistent native input fence. This does not grant rotation
 * or a second input owner. Configure before the first service dispatch. */
typedef BOOL (*SudekiMpLanStoryHostLeaderAiExact)(
    const SudekiMpControlUpdateDispatchWitness *,const SudekiMpLanStoryNativeRoster *);
BOOL SudekiMpLanStoryHostControlSetLeaderAiWitness(SudekiMpLanStoryHostLeaderAiExact);
BOOL SudekiMpLanStoryHostControlLeaderActionsDrained(
    const SudekiMpControlUpdateDispatchWitness *,const SudekiMpLanStoryNativeRoster *);
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
/* Existing companion ownership depends on the exact host actor binding,
 * not on whether a native interaction currently accepts host input. This
 * proves the same roster/AI/controller ownership as Ready, but does not
 * authorize acquisition or movement through a foreign input filter. */
BOOL SudekiMpLanStoryHostControlBound(void *controller,
    const SudekiMpControlUpdateDispatchWitness *witness,
    const SudekiMpLanStoryScene *scene);
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
