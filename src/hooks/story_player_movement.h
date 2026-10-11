#ifndef SUDEKIMP_STORY_PLAYER_MOVEMENT_H
#define SUDEKIMP_STORY_PLAYER_MOVEMENT_H
#include <windows.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include "network/lan_story_handoff.h"

/* Retail input scaling is 2 - action2B. The controller then shapes the
 * lateral/vertical pair (vertical is zero), leaving forward input scaled.
 * Confirmed by executing 428B00 with inert camera/Move sinks. Do not clamp
 * the resulting horizontal magnitude to one or scale all directions alike. */
static inline BOOL SudekiMpStoryMovementLocal(float x,float z,float out[3]) {
    /* Exact float constants avoid x87 retaining extra decimal precision at
     * the inclusive native deadzone and scaled-axis boundaries. */
    if(!out || !isfinite(x) || !isfinite(z) ||
        fabsf(x)>0x1.00418ap+2f || fabsf(z)>0x1.00418ap+2f) return FALSE;
    out[0]=out[1]=out[2]=0.0f;
    if(fabsf(x)<=0x1.0624dep-10f && fabsf(z)<=0x1.0624dep-10f) return TRUE;
    out[0]=fmaxf(-1.0f,fminf(1.0f,x)); out[2]=z;
    return TRUE;
}
static inline BOOL SudekiMpStoryMovementVector(float x,float z,float out[3],float *speed) {
    if(!out || !speed || !SudekiMpLanStoryMovementVectorValid(x,z)) return FALSE;
    float n=sqrtf(x*x+z*z);
    out[0]=out[1]=out[2]=0.0f; *speed=n;
    if(n>0.0f) { out[0]=x/n; out[2]=z/n; }
    return TRUE;
}
/* ABI-only submission: caller owns the exact actor/lease and C2 turn scope.
 * Native Move checks CanMove and the movement write-enable itself. A normal
 * key release goes through native blended SetSpeed; lifecycle drains retain
 * their separate immediate-stop contract. */
static inline void SudekiMpStoryMovementSubmit(uint8_t *image,void *arbiter,
    const float heading[3],float speed) {
    if(speed>0.0f)
        ((void (__stdcall *)(void *,const float *,float,float,unsigned))(image+0xdae80u))(
            arbiter,heading,speed,1.0f,0u);
    else
        ((void (__attribute__((thiscall)) *)(void *,float,float))(image+0xdb070u))(arbiter,0.0f,1.0f);
}

/* Named, relocation-normalized contracts. No new native patch seam. */
static const struct { const char *name; unsigned rva,size; uint32_t hash;
    unsigned relocation; unsigned relocations; } story_movement_contracts[]={
    {"C4_INPUT_SCALE",0x278d9u,0x5fu,0xfe5c1f3fu,0x16u,1},
    {"C4_LOCAL_SHAPE",0x289d0u,0x12bu,0xc2fde6d7u,0x8bu,1},
    {"C4_CONTROLLER_DEADZONE",0x28b4bu,0x55u,0x1859a236u,0x12u,1},
    {"C4_CONTROLLER_VECTOR",0x28bfdu,0x27u,0x124f55c4u,0,0},
    {"C4_CONTROLLER_MAGNITUDE",0x28d33u,0x70u,0x3977c241u,0,0},
    {"C4_MOVE_ARBITRATION",0xdae80u,0xf9u,0xa5c49663u,0,0},
    {"C4_BLEND_STOP",0xdb070u,0x70u,0x713d944du,0,0},
    {"C4_CAN_MOVE",0xdb800u,0x74u,0x2911b758u,0x48u,1}
};
static inline BOOL SudekiMpStoryMovementImageExact(uint8_t *image,
    BOOL (*readable)(const void *,size_t)) {
    if(!image || !readable) return FALSE;
    for(unsigned k=0;k<sizeof(story_movement_contracts)/sizeof(*story_movement_contracts);++k) {
        const __typeof__(*story_movement_contracts) *c=&story_movement_contracts[k];
        if(!readable(image+c->rva,c->size)) return FALSE;
        uint32_t h=2166136261u,operand=0;
        if(c->relocations) {
            memcpy(&operand,image+c->rva+c->relocation,4);
            operand-=(uint32_t)(uintptr_t)image-0x400000u;
        }
        for(unsigned i=0;i<c->size;++i) {
            uint8_t b=image[c->rva+i];
            if(c->relocations && i>=c->relocation && i<c->relocation+4u)
                b=(uint8_t)(operand>>(8u*(i-c->relocation)));
            h=(h^b)*16777619u;
        }
        if(h!=c->hash) return FALSE;
    }
    return readable(image+0x2e35ccu,4) && *(uint32_t *)(image+0x2e35ccu)==0xbf800000u &&
        readable(image+0x2e3628u,8) && *(uint32_t *)(image+0x2e3628u)==0 &&
        *(uint32_t *)(image+0x2e362cu)==0x40000000u &&
        readable(image+0x2c02d8u,4) && *(uint32_t *)(image+0x2c02d8u)==0x3a83126fu;
}
#endif
