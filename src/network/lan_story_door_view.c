#include "network/lan_story_door_view.h"
#include <math.h>
#include <string.h>

static BOOL finite3(const float v[3]) { return isfinite(v[0]) && isfinite(v[1]) && isfinite(v[2]); }
static float side_of(const float forward[3],const float door[3],const float point[3]) {
    return (point[0]-door[0])*forward[0]+(point[2]-door[2])*forward[2];
}
BOOL SudekiMpStoryDoorViewSelect(const float door_position[3],const float door_forward[3],
    const float actor_position[3],float traverse,float margin,
    const SudekiMpStoryDoorCamera cameras[2],unsigned *chosen) {
    if(!door_position || !door_forward || !actor_position || !cameras || !chosen ||
        !finite3(door_position) || !finite3(door_forward) || !finite3(actor_position) ||
        !isfinite(traverse) || traverse<=0 || traverse>50.0f || !isfinite(margin) || margin<0) return FALSE;
    float flat=sqrtf(door_forward[0]*door_forward[0]+door_forward[2]*door_forward[2]);
    if(!isfinite(flat) || flat<0.01f) return FALSE;
    float dx=actor_position[0]-door_position[0],dz=actor_position[2]-door_position[2];
    float distance=sqrtf(dx*dx+dz*dz);
    if(!isfinite(distance) || distance>traverse+margin) return FALSE;
    float actor_side=side_of(door_forward,door_position,actor_position);
    unsigned pick=2u,any=2u;
    for(unsigned i=0;i<2u;++i) {
        if(!cameras[i].valid || !finite3(cameras[i].position) || !finite3(cameras[i].direction)) continue;
        if(any==2u) any=i;
        float camera_side=side_of(door_forward,door_position,cameras[i].position);
        if((camera_side<0)==(actor_side<0) && fabsf(camera_side)>0.01f) { pick=i; break; }
    }
    if(pick==2u) pick=any;
    if(pick==2u) return FALSE;
    *chosen=pick; return TRUE;
}
BOOL SudekiMpStoryDoorViewBuild(const SudekiMpStoryDoorCamera *camera,const float actor_position[3],
    const float projection[3],SudekiMpLanStoryView *out) {
    if(!camera || !camera->valid || !actor_position || !projection || !out ||
        !finite3(camera->position) || !finite3(camera->direction) || !finite3(actor_position) ||
        !finite3(projection)) return FALSE;
    float forward[3];
    if(camera->fixed_direction) memcpy(forward,camera->direction,sizeof(forward));
    else {
        forward[0]=actor_position[0]-camera->position[0];
        forward[1]=actor_position[1]+1.8f-camera->position[1];
        forward[2]=actor_position[2]-camera->position[2];
    }
    float length=sqrtf(forward[0]*forward[0]+forward[1]*forward[1]+forward[2]*forward[2]);
    if(!isfinite(length) || length<0.1f) return FALSE;
    for(unsigned i=0;i<3u;++i) forward[i]/=length;
    float flat=sqrtf(forward[0]*forward[0]+forward[2]*forward[2]);
    if(flat<0.001f) return FALSE;
    float right[3]={-forward[2]/flat,0,forward[0]/flat};
    float up[3]={-right[2]*forward[1],right[2]*forward[0]-right[0]*forward[2],right[0]*forward[1]};
    SudekiMpLanStoryView view={0};
    memcpy(view.matrix,right,12u); memcpy(view.matrix+4u,up,12u);
    memcpy(view.matrix+8u,forward,12u); memcpy(view.matrix+12u,camera->position,12u); view.matrix[15]=1;
    memcpy(view.projection,projection,sizeof(view.projection));
    view.valid=1;
    if(!SudekiMpLanStoryViewGeometryValid(&view)) return FALSE;
    *out=view; return TRUE;
}
