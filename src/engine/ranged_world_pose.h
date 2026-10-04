#ifndef SUDEKIMP_RANGED_WORLD_POSE_H
#define SUDEKIMP_RANGED_WORLD_POSE_H
#include <math.h>

/* Authored first-person semantic -> world semantic, not renderer selectors.
 * Resource presence and source identity must still be proved by the caller.
 * The established ranged adapter uses these missile combo families. */
static inline unsigned SudekiMpRangedWorldSemantic(unsigned semantic) {
    switch(semantic) {
    case 0x05u:return 0x02u;
    case 0x8cu:return 0x85u;
    case 0x8du:return 0x86u;
    case 0x8eu:return 0x87u;
    default:return semantic;
    }
}
/* Banks may author a different number of frames for the same action. Keep
 * normalized phase and playback duration; do not clamp an invalid source
 * clock into an apparently valid world pose. Empty clips are handled outside. */
static inline int SudekiMpRangedWorldClock(float source_length,float world_length,
    float source_time,float source_rate,float *world_time,float *world_rate) {
    if(!world_time || !world_rate || !isfinite(source_length) || source_length<=0 ||
        !isfinite(world_length) || world_length<=0 || !isfinite(source_time) ||
        source_time<0 || source_time>source_length || !isfinite(source_rate)) return 0;
    float scale=world_length/source_length,t=source_time*scale,r=source_rate*scale;
    if(!isfinite(t) || !isfinite(r) || t>world_length+.01f) return 0;
    *world_time=t; *world_rate=r; return 1;
}
#endif
