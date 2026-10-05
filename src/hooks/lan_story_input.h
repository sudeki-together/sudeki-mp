#ifndef SUDEKIMP_LAN_STORY_INPUT_H
#define SUDEKIMP_LAN_STORY_INPUT_H

#include "hooks/control_separation.h"

/* Client-only native character-input fence. Install before arming the native
 * saved-game load. Owns the pristine CCharacterController input-listener slot,
 * so it cannot coexist with the arena/SMP4 client-input adapter. The controller
 * input listener is a subobject at +0x2c, not the controller's primary object.
 * Other title, menu and camera listener slots are unchanged. */
BOOL SudekiMpLanStoryInputInstall(HMODULE image, unsigned client_player);
/* Seed the native thread from the existing exact post-controller seam, before
 * asking the separate story containment adapter to observe the loaded world.
 * The hook already suppresses local native character input before this call. */
BOOL SudekiMpLanStoryInputObserve(void *controller,
    const SudekiMpControlUpdateDispatchWitness *witness);
/* Same verified native thread, outside input callbacks. Freshly proves the
 * pointer-hook owner, both controller vtables, global controller and current
 * actor target. It grants no scheduler, actor, pose or camera authority and
 * does not claim that older cached input or native actions have drained. */
BOOL SudekiMpLanStoryInputExact(void *controller, void *actor);
/* Movement sampling remains separate from the closed native controller path.
 * Only this window's verified character-listener events feed these axes.
 * Binding a new host transaction clears held state; focus loss and menus must
 * close it. No keyboard polling, native action or network send occurs here. */
BOOL SudekiMpLanStoryInputArm(void *controller,void *actor,uint32_t transaction);
BOOL SudekiMpLanStoryInputSample(void *controller,void *actor,uint32_t transaction,
    float *local_x,float *local_z);
/* Consume this window's native action19 press edge. No native controller
 * cache is seeded. Releases/repeats cannot open the menu a second time. */
BOOL SudekiMpLanStoryInputTakeQuickMenu(void *controller,void *actor,uint32_t transaction);
/* Fresh window-owned 2C/2D/2E press, 1 weak/2 strong/3 sweep, else zero.
 * Four bounded edges, each expires after 250ms. Focus/menu/rebind discards the
 * queue without making a held key a new press. No native action execution. */
unsigned SudekiMpLanStoryInputTakeMelee(void *controller,void *actor,uint32_t transaction);
/* Keep the exact binding while a local menu owns navigation. Discard held
 * movement/look/attack edges so closing it cannot replay old gameplay input. */
void SudekiMpLanStoryInputMuteMovement(void);
/* Consume elapsed time once per local presentation. Uses window-owned 69/6A
 * events and retail Exploration sensitivity/inversion. Outputs bounded axis
 * seconds; the view applies native authored rotation and distance settings.
 * Drops stale axes and long gaps; no native tick. */
BOOL SudekiMpLanStoryInputOrbit(void *controller,void *actor,uint32_t transaction,
    float *horizontal_seconds,float *vertical_seconds);
void SudekiMpLanStoryInputClear(void);
/* Caller first retires all presentation/containment users of Exact. Failed
 * restoration retains the original callback and hook metadata for retry. */
BOOL SudekiMpLanStoryInputUninstall(void);

#endif
