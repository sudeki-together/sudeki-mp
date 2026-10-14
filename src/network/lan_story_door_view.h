#ifndef SUDEKIMP_LAN_STORY_DOOR_VIEW_H
#define SUDEKIMP_LAN_STORY_DOOR_VIEW_H
#include "network/lan_story_frame.h"
#include <windows.h>

/* Door camera (#67 H1b). Pure math shared by the host capture and its tests:
 * which of a door's two authored static cameras a character approaching the
 * door would get, and the camera-to-world view the client presents for it.
 * Matrix rows: right, up, forward, eye (the local-control convention). */
typedef struct SudekiMpStoryDoorCamera {
    BOOL valid;
    BOOL fixed_direction;         /* record +0x2C: 1 = authored look direction */
    float position[3];            /* record +0x08 */
    float direction[3];           /* record +0x14, unit */
} SudekiMpStoryDoorCamera;

/* TRUE when `actor` is within `traverse` (+margin) of the door in the ground
 * plane and a camera on the actor's side of the door plane exists; `chosen`
 * is 0 (front slot, CDoor+0x78) or 1 (back slot, CDoor+0x84). The native
 * lead path picks the camera standing on the same side the lead approaches
 * from (CONFIRMED_LIVE, TallGate), so the character walks away from it. */
BOOL SudekiMpStoryDoorViewSelect(const float door_position[3],const float door_forward[3],
    const float actor_position[3],float traverse,float margin,
    const SudekiMpStoryDoorCamera cameras[2],unsigned *chosen);
/* Camera-to-world view at the record's position. A fixed-direction record
 * looks along its authored direction; otherwise it looks at the actor's
 * native target point (+1.8 above the actor). Projection is copied. */
BOOL SudekiMpStoryDoorViewBuild(const SudekiMpStoryDoorCamera *camera,const float actor_position[3],
    const float projection[3],SudekiMpLanStoryView *out);
#endif
