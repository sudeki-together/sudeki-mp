#ifndef SUDEKIMP_STORY_AIMED_FLASH_H
#define SUDEKIMP_STORY_AIMED_FLASH_H
#include <math.h>
#include <stdint.h>

/* Weapon fire effects placed without locator follow ("Fire Effect follows
 * locator"=0): native CMissileManager 0x4C7D20, branch 0x4C7E23-0x4C7EC6,
 * builds world rows X=(dz,0,-dx), Y=(0,1,0), Z=d (the aim) and the fire
 * position. Whenever the aim is pitched the basis is sheared, so it has no
 * rotation+scale pose and used to fail the whole story VFX capture for the
 * effect's ~2 s life (Xuric's Shock Staff: the owner's "lightning").
 * CONFIRMED_STATIC (disassembly; game data for the four kinds). The wire pose
 * carries the orthonormal frame {X^, Z^ x X^, Z^} and the three row lengths;
 * the client rebuilds rows X and Y from Z exactly as the native routine does.
 * Ailish (owner type 0x01) kinds 36 SFXAF004_XURICS_FIRE_EFFECT and 38
 * SFXAF006_WIZARDWOOD_STAVE_FIRE_EFFECT; Elco (0x0E) kinds 56
 * SFXEF005_ELUVIAM_FIRE_EFFECT and 57 SFXEF007_SHOOTING_IRON_FIRE_EFFECT;
 * ordinary weapon fire only (skill sequence 0). Pure. */
static inline int SudekiMpAimedFlashKind(uint8_t owner_type,uint8_t kind,uint32_t skill_sequence) {
    if(skill_sequence) return 0;
    if(owner_type==0x01u) return kind==36u || kind==38u;
    if(owner_type==0x0eu) return kind==56u || kind==57u;
    return 0;
}
static inline float sudekimp_aimed_length(const float *r) { return sqrtf(r[0]*r[0]+r[1]*r[1]+r[2]*r[2]); }
/* `in` matches the native aimed basis (Y straight up, X along (Z.z,0,-Z.x),
 * a non-vertical aim): writes the orthonormal frame scaled by the native row
 * lengths to `out` (same position) and returns 1. Anything else returns 0. */
static inline int SudekiMpAimedFlashOrthonormalize(const float in[16],float out[16]) {
    const float eps=0.002f;
    for(unsigned i=0;i<16u;++i) if(!isfinite(in[i])) return 0;
    float s0=sudekimp_aimed_length(in),s1=sudekimp_aimed_length(in+4),s2=sudekimp_aimed_length(in+8);
    if(!(s0>0.0001f) || !(s1>0.0001f) || !(s2>0.0001f) || s0>1000.0f || s1>1000.0f || s2>1000.0f) return 0;
    float x[3]={in[0]/s0,in[1]/s0,in[2]/s0},y[3]={in[4]/s1,in[5]/s1,in[6]/s1},z[3]={in[8]/s2,in[9]/s2,in[10]/s2};
    float h=sqrtf(z[0]*z[0]+z[2]*z[2]);
    if(!(h>0.05f) || fabsf(y[0])>eps || fabsf(y[2])>eps || y[1]<1.0f-eps || fabsf(x[1])>eps ||
        fabsf(x[0]-z[2]/h)>eps || fabsf(x[2]+z[0]/h)>eps) return 0;
    /* Y' = Z x X: the native up row tilted with the aim (equal at zero pitch). */
    float u[3]={z[1]*x[2]-z[2]*x[1],z[2]*x[0]-z[0]*x[2],z[0]*x[1]-z[1]*x[0]};
    for(unsigned i=0;i<16u;++i) out[i]=in[i];
    for(unsigned k=0;k<3u;++k) { out[k]=x[k]*s0; out[4u+k]=u[k]*s1; out[8u+k]=z[k]*s2; }
    return 1;
}
/* Client: a matrix rebuilt from the wire pose of an aimed flash gets the
 * native rows back: X=(Z.z,0,-Z.x)/|.|*|X|, Y=(0,|Y|,0), Z and position kept. */
static inline void SudekiMpAimedFlashRebuild(float m[16]) {
    float s0=sudekimp_aimed_length(m),s1=sudekimp_aimed_length(m+4),s2=sudekimp_aimed_length(m+8);
    if(!(s2>0.0001f)) return;
    float zx=m[8]/s2,zz=m[10]/s2,h=sqrtf(zx*zx+zz*zz);
    if(!(h>0.0001f)) return;
    m[0]=zz/h*s0; m[1]=0.0f; m[2]=-zx/h*s0;
    m[4]=0.0f; m[5]=s1; m[6]=0.0f;
}
#endif
