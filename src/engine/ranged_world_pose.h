#ifndef SUDEKIMP_RANGED_WORLD_POSE_H
#define SUDEKIMP_RANGED_WORLD_POSE_H
#include <math.h>
#include <stdint.h>

/* Keep an observer's body upright while retaining the full aim separately.
 * At an exactly vertical aim use the already owned body's horizontal heading,
 * not a fabricated direction. Output is transactional on invalid input. */
static inline int SudekiMpRangedWorldRoot(const float aim[3],const float prior[3],float root[3]) {
    if(!aim || !prior || !root) return 0;
    float norm=0;
    for(unsigned i=0;i<3;++i) { if(!isfinite(aim[i])) return 0;norm+=aim[i]*aim[i]; }
    if(!isfinite(norm) || norm<.5f || norm>1.5f) return 0;
    float x=aim[0],z=aim[2],flat=x*x+z*z;
    if(flat<.00000001f) {
        x=prior[0];z=prior[2];flat=x*x+z*z;
        if(!isfinite(x) || !isfinite(z) || !isfinite(flat) || flat<.00000001f) return 0;
    }
    float length=sqrtf(flat),out[3]={x/length,0,z/length};
    for(unsigned i=0;i<3;++i) root[i]=out[i];
    return 1;
}

/* Authored first-person semantic -> world semantic, not renderer selectors.
 * Resource presence and source identity must still be proved by the caller.
 * The established ranged adapter uses these missile combo families. */
static inline unsigned SudekiMpRangedWorldSemantic(unsigned semantic) {
    switch(semantic) {
    case 0x05u:return 0x02u;
    /* Native D2598 selects the arms-only get-hit (36) while FP remains
     * active. Its body counterpart is the authored weak flinch (2a).
     * Knockdowns leave FP before selection and retain their distinct native
     * body clips. Translate presentation only, never damage or control. */
    case 0x36u:return 0x2au;
    case 0x8cu:return 0x85u;
    case 0x8du:return 0x86u;
    case 0x8eu:return 0x87u;
    default:return semantic;
    }
}
/* Body locomotion families from native E3D10: forward6/7, back8/9,
 * leftA/B, rightC/D. Observe actual displacement relative to the aim;
 * this chooses presentation only and never rotates or moves the actor. */
static inline int SudekiMpRangedWorldMoveSemantic(const float aim[3],
    float dx,float dz,unsigned *semantic) {
    if(!aim || !semantic || !isfinite(dx) || !isfinite(dz) ||
        !isfinite(aim[0]) || !isfinite(aim[1]) || !isfinite(aim[2])) return 0;
    float flat=aim[0]*aim[0]+aim[2]*aim[2],distance=dx*dx+dz*dz;
    if(!isfinite(flat) || flat<.00000001f || !isfinite(distance) || distance<=0) return 0;
    /* Use the Test Room's native handedness (bc56ed8), not an assumed
     * world-right basis. Positive lateral selects arbiter mode2; C3B70
     * maps that to movement state3 and E3D10 to A/B. Negative lateral
     * selects mode3 -> state4 -> C/D. Targeting screen-right is -Z,+X. */
    float forward=dx*aim[0]+dz*aim[2],lateral=dx*aim[2]-dz*aim[0];
    if(!isfinite(forward) || !isfinite(lateral)) return 0;
    *semantic=fabsf(lateral)>fabsf(forward) ? (lateral>0?0xau:0xcu) : (forward>=0?6u:8u);
    return 1;
}
/* Native E1560 reads each animation definition's half-float multiplier;
 * E3D10 blends the two authored cycle durations, then synchronizes both
 * channels to that duration. Forward-run rates cannot be reused for a
 * different gait: Ailish's sideways run has no forward-run speed boost. */
static inline int SudekiMpRangedWorldGaitRates(const float length[2],
    const uint16_t multiplier[2],float blend,float rate[2]) {
    if(!length || !multiplier || !rate || !isfinite(blend) || blend<0 || blend>1) return 0;
    float period[2];
    for(unsigned c=0;c<2;++c) {
        unsigned exponent=(multiplier[c]>>10)&31u;
        if(!isfinite(length[c]) || length[c]<1 || (multiplier[c]&0x8000u) ||
            !exponent || exponent==31u) return 0;
        float speed=ldexpf(1.f+(multiplier[c]&0x3ffu)/1024.f,(int)exponent-15);
        period[c]=length[c]/(24.f*speed);
    }
    float duration=period[0]+(period[1]-period[0])*blend;
    float first=length[0]/duration,second=length[1]/duration;
    if(!isfinite(duration) || duration<=0 || !isfinite(first) || !isfinite(second)) return 0;
    rate[0]=first;rate[1]=second;return 1;
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
