#include <winsock2.h>
#include "network/lan_story_door_view.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
/* Live TallGate numbers (2026-10-13): door at (-127.35,6.72,-64.42) facing
 * (0.654,0,-0.757); camera af48 (back slot) on the +side, af49 (front slot)
 * on the -side; Ailish approaching from the -side got af49. */
int main(void) {
    const float door[3]={-127.35f,6.72f,-64.42f},forward[3]={0.654f,0,-0.757f};
    SudekiMpStoryDoorCamera cams[2]={
        {TRUE,TRUE,{-134.367493f,11.144548f,-56.063709f},{0.5992f,-0.4160f,-0.6840f}}, /* +0x78 front slot, af49 */
        {TRUE,TRUE,{-121.610550f,9.571011f,-69.838631f},{-0.6883f,-0.1938f,0.6974f}}}; /* +0x84 back slot, af48 */
    unsigned chosen=9;
    float ailish_minus[3]={-128.2f,6.7f,-63.3f}; /* side -1.3 */
    assert(SudekiMpStoryDoorViewSelect(door,forward,ailish_minus,5.0f,0.5f,cams,&chosen) && chosen==0);
    float ailish_plus[3]={-126.0f,6.7f,-66.0f};
    assert(SudekiMpStoryDoorViewSelect(door,forward,ailish_plus,5.0f,0.5f,cams,&chosen) && chosen==1);
    float distant[3]={-120.0f,6.7f,-60.0f};
    assert(!SudekiMpStoryDoorViewSelect(door,forward,distant,5.0f,0.5f,cams,&chosen));
    /* Only one authored camera (portcullis has no front): still usable. */
    SudekiMpStoryDoorCamera one[2]={{FALSE},cams[1]};
    assert(SudekiMpStoryDoorViewSelect(door,forward,ailish_minus,5.0f,0.5f,one,&chosen) && chosen==1);
    SudekiMpStoryDoorCamera none[2]={{FALSE},{FALSE}};
    assert(!SudekiMpStoryDoorViewSelect(door,forward,ailish_minus,5.0f,0.5f,none,&chosen));
    assert(!SudekiMpStoryDoorViewSelect(door,forward,ailish_minus,0,0.5f,cams,&chosen));
    float bad[3]={NAN,0,0};
    assert(!SudekiMpStoryDoorViewSelect(door,forward,bad,5.0f,0.5f,cams,&chosen));
    SudekiMpLanStoryView view; const float projection[3]={1.1344640f,0.01f,1000.0f};
    assert(SudekiMpStoryDoorViewBuild(&cams[0],ailish_minus,projection,&view));
    assert(view.valid && fabsf(view.matrix[12]+134.367493f)<.001f && fabsf(view.matrix[10]+0.684f)<.01f &&
        fabsf(view.matrix[5])>0.5f && view.matrix[15]==1 && view.projection[2]==1000.0f);
    /* Camera looks at the gate: forward dotted with (door - eye) is positive. */
    float to_door[3]={door[0]-view.matrix[12],door[1]-view.matrix[13],door[2]-view.matrix[14]};
    assert(to_door[0]*view.matrix[8]+to_door[1]*view.matrix[9]+to_door[2]*view.matrix[10]>0);
    SudekiMpStoryDoorCamera tracking=cams[1]; tracking.fixed_direction=FALSE;
    assert(SudekiMpStoryDoorViewBuild(&tracking,ailish_plus,projection,&view));
    float to_actor[3]={ailish_plus[0]-view.matrix[12],ailish_plus[1]+1.8f-view.matrix[13],ailish_plus[2]-view.matrix[14]};
    float n=sqrtf(to_actor[0]*to_actor[0]+to_actor[1]*to_actor[1]+to_actor[2]*to_actor[2]);
    assert(fabsf(view.matrix[8]-to_actor[0]/n)<.001f && fabsf(view.matrix[10]-to_actor[2]/n)<.001f);
    SudekiMpStoryDoorCamera invalid=cams[0]; invalid.valid=FALSE;
    assert(!SudekiMpStoryDoorViewBuild(&invalid,ailish_minus,projection,&view));
    puts("story door view: side selection and camera-to-world build passed");
    return 0;
}
