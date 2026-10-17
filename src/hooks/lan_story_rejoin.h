#ifndef SUDEKIMP_LAN_STORY_REJOIN_H
#define SUDEKIMP_LAN_STORY_REJOIN_H
#include <math.h>
/* H1b: where a member who was already inside a house goes back to after the
 * lead's own entry reloaded it (the member stood there on their own screen
 * the whole time). The recorded spot, unless it overlaps the lead's arrival:
 * then pushed out from the lead to `clear` (xz), height kept. Pure. */
enum { SUDEKIMP_REJOIN_RECORDED=0, SUDEKIMP_REJOIN_CLEARED=1, SUDEKIMP_REJOIN_INVALID=2 };
static inline unsigned SudekiMpLanStoryRejoinSpot(const float recorded[3],const float *lead,float clear,float out[3]) {
    if(!recorded || !out || !isfinite(recorded[0]) || !isfinite(recorded[1]) || !isfinite(recorded[2]) ||
        !isfinite(clear) || clear<0) return SUDEKIMP_REJOIN_INVALID;
    out[0]=recorded[0]; out[1]=recorded[1]; out[2]=recorded[2];
    if(!lead || !isfinite(lead[0]) || !isfinite(lead[2])) return SUDEKIMP_REJOIN_RECORDED;
    float dx=recorded[0]-lead[0],dz=recorded[2]-lead[2],d=sqrtf(dx*dx+dz*dz);
    if(d>=clear) return SUDEKIMP_REJOIN_RECORDED;
    if(d<1e-4f) { dx=1.0f; dz=0.0f; d=1.0f; }
    out[0]=lead[0]+dx/d*clear; out[2]=lead[2]+dz/d*clear;
    return SUDEKIMP_REJOIN_CLEARED;
}
#endif
