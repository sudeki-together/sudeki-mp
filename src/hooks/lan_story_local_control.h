#ifndef SUDEKIMP_LAN_STORY_LOCAL_CONTROL_H
#define SUDEKIMP_LAN_STORY_LOCAL_CONTROL_H

#include "hooks/lan_story_observer.h"
#include "network/lan_story_frame.h"

/* The containment owner accepts only its retained native party actors,
 * unchanged world/descriptor/group/controller and registry/pause/task
 * ownership, with a physically selected party member. This is a synchronous
 * post-controller transaction; no borrowed callback survives the call.
 * Local scene revision may advance with the native physical front, but its
 * epoch must remain unchanged. No network offer is restamped here. */
typedef BOOL (*SudekiMpLanStoryLocalControlExact)(
    const SudekiMpLanStoryNativeRoster *,const SudekiMpLanStoryScene *,void *context);
typedef struct SudekiMpLanStoryLocalControlReport {
    SudekiMpLanStoryNativeRoster roster;
    SudekiMpLanStoryScene scene;
    BOOL entered,coherent,bound,camera_exact,unknown;
    const char *reason;
} SudekiMpLanStoryLocalControlReport;

BOOL SudekiMpLanStoryLocalControlInstall(HMODULE image);
/* Original native spectator view plus the actor position captured at THAT
 * same first borrow. Never subtract today's replicated actor from an old
 * saved camera. Caller obtains this under its retained Replica scope before
 * restoring that view. Copies plain data only; no native camera mutation. */
BOOL SudekiMpLanStoryLocalControlSeedView(const SudekiMpLanStoryView *view,
    const float original_actor_position[3]);
/* Caller must first finish recruitment presentation and retire the old
 * Replica view lease. The native client input fence and full world pause
 * stay held. TRUE means the returned fresh native binding and camera were
 * proved, and the caller must adopt this exact local roster/scene before
 * allowing further presentation or acknowledging host control. */
BOOL SudekiMpLanStoryLocalControlSwitch(void *controller,
    const SudekiMpControlUpdateDispatchWitness *witness,
    const SudekiMpLanStoryNativeRoster *before,
    const SudekiMpLanStoryScene *before_scene,unsigned character,
    SudekiMpLanStoryLocalControlExact exact,void *context,
    SudekiMpLanStoryLocalControlReport *report);
BOOL SudekiMpLanStoryLocalControlReady(void *controller,
    const SudekiMpControlUpdateDispatchWitness *witness,
    const SudekiMpLanStoryNativeRoster *roster,
    const SudekiMpLanStoryScene *scene);
typedef BOOL (*SudekiMpLanStoryLocalViewExact)(
    const SudekiMpLanStoryNativeRoster *,void *context);
/* Same paused ClientPresent transaction, after replicated actor positions.
 * Mathematical view centered on the selected character using her native target offset and
 * the camera's authored Exploration rotation/distance/height settings.
 * Inputs are sensitivity-scaled axis seconds. Native collision avoidance,
 * return steering and spring smoothing are not executed by this adapter.
 * No native camera tick, world simulation or actor identity writes. Ready
 * stays FALSE until at least one successful local view publication. The
 * borrowed view is retired only after the caller's native Quit witness. */
/* Dev Play ally seat: the published view frames this entity (NULL = the selected hero). */
void SudekiMpLanStoryLocalControlSetAnchorEntity(void *entity);
BOOL SudekiMpLanStoryLocalControlPresent(const SudekiMpLanStoryNativeRoster *roster,
    SudekiMpLanStoryLocalViewExact exact,void *context,float horizontal_seconds,float vertical_seconds);
/* Read-only local movement basis. Caller still owns transport offer/ACK,
 * scene freshness and the independent paused-world containment proof. */
BOOL SudekiMpLanStoryLocalControlDirection(void *controller,
    const SudekiMpControlUpdateDispatchWitness *witness,
    const SudekiMpLanStoryNativeRoster *roster,const SudekiMpLanStoryScene *scene,
    float local_x,float local_z,float *world_x,float *world_z);
BOOL SudekiMpLanStoryLocalControlRetains(void);
/* An unknown attempted transition is never forgotten until the existing
 * native Quit return positively proves the old world was destroyed. */
BOOL SudekiMpLanStoryLocalControlNativeExitReturned(void);
BOOL SudekiMpLanStoryLocalControlUninstall(void);

#endif
