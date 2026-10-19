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
/* One admitted fresh press of the native main-menu action (0x18, M). */
BOOL SudekiMpLanStoryInputTakeMainMenu(void *controller,void *actor,uint32_t transaction);
/* Same press, but only focus-gated: lets a shown native menu be closed even
 * when input admission has lapsed. */
BOOL SudekiMpLanStoryInputTakeMainMenuRaw(void);
/* Research: log every native event seen by the controller listener hook (capped). */
void SudekiMpLanStoryInputTraceEvents(BOOL on);
/* One native frame of 2C/2D/2E states, packed as weak|strong<<2|sweep<<4.
 * Press=1, held=2, release=3, neutral=0; simultaneous events stay together.
 * generation changes on focus/menu/rebind so unsent history can be retired. */
BOOL SudekiMpLanStoryInputMelee(void *controller,void *actor,uint32_t transaction,
    uint8_t *buttons,uint32_t *generation);
/* Drop the weak-attack states still pending after a press the caller kept
 * (a shopkeeper talk prompt): no held or release state reaches the host. */
void SudekiMpLanStoryInputConsumeWeak(void);
/* Keep the exact binding while a local menu owns navigation. Discard held
 * movement/look/attack edges so closing it cannot replay old gameplay input. */
BOOL SudekiMpLanStoryInputRanged(void *controller,void *actor,uint32_t transaction,
    BOOL *held,uint32_t *press);
/* Native action31, held only in the exact focused binding. */
BOOL SudekiMpLanStoryInputBlock(void *controller,void *actor,uint32_t transaction);
void SudekiMpLanStoryInputMuteMovement(void);
/* Same gameplay discard during an unchanged-area media gap, preserving only
 * independently focused/fresh camera axes and their ORIGINAL timestamps. */
void SudekiMpLanStoryInputMuteForCamera(void);
/* Consume elapsed time once per local presentation. Uses window-owned 69/6A
 * events and retail Exploration sensitivity/inversion. Outputs bounded axis
 * seconds; the view applies native authored rotation and distance settings.
 * Drops stale axes and long gaps; no native tick. */
BOOL SudekiMpLanStoryInputOrbit(void *controller,void *actor,uint32_t transaction,
    float *horizontal_seconds,float *vertical_seconds);
void SudekiMpLanStoryInputClear(void);
BOOL SudekiMpLanStoryInputAwayLook(void *controller,void *actor,BOOL enabled);
/* Latest focused right-stick look axes (native 69/6A magnitudes, -1..1), or
 * zeros when stale/unfocused. Read-only; nothing is consumed. */
BOOL SudekiMpLanStoryInputLookAxes(float *x,float *y);
/* Caller first retires all presentation/containment users of Exact. Failed
 * restoration retains the original callback and hook metadata for retry. */
BOOL SudekiMpLanStoryInputUninstall(void);

#endif
