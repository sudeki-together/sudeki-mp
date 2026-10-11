#ifndef SUDEKIMP_STORY_TARGET_REFRESH_H
#define SUDEKIMP_STORY_TARGET_REFRESH_H

#include <windows.h>
#include <stdint.h>
#include <string.h>
#include <math.h>

/* #54 C1. Caller owns the current game-thread, world and remote actor lease.
 * The retail melee search takes CTargeter in EAX (no stack arguments). It
 * scores against +48, queries the current world, and commits via B9D20's
 * intrusive-pointer helper. Unlike B9AB0/B9680 it never touches lock-on UI.
 * No code is patched and no controller/lead/AI/lock flag is substituted. */
enum { STORY_TARGET_ACQUIRE=0xba1c0u, STORY_TARGET_COMMIT=0xb9d20u,
    STORY_TARGET_EXCLUDE=0xbaad0u, STORY_TARGET_WORLD=0x408d10u,
    STORY_TARGET_COLLISION=0x408dd4u };
typedef BOOL (*SudekiMpStoryTargetMemory)(const void *,size_t,BOOL);

static inline BOOL SudekiMpStoryTargetHash(uint8_t *base,unsigned rva,unsigned size,
    const unsigned *relocs,unsigned count,uint32_t expected,SudekiMpStoryTargetMemory memory) {
    uint32_t hash=2166136261u; unsigned next=0;
    if(!base || !memory || !memory(base+rva,size,FALSE)) return FALSE;
    for(unsigned i=0;i<size;) {
        if(next<count && i==relocs[next]) {
            uint32_t value; memcpy(&value,base+rva+i,4u);
            value-=(uint32_t)(uintptr_t)base;
            for(unsigned b=0;b<4u;++b) hash=(hash^((value>>(8u*b))&255u))*16777619u;
            i+=4u; ++next;
        } else hash=(hash^base[rva+i++])*16777619u;
    }
    return hash==expected;
}
static inline BOOL SudekiMpStoryTargetImageExact(uint8_t *base,SudekiMpStoryTargetMemory memory) {
    static const unsigned acquisition_relocs[]={0x154u,0x23eu,0x29du,0x2f0u,0x380u,0x3e5u};
    return SudekiMpStoryTargetHash(base,STORY_TARGET_ACQUIRE,0x504u,
            acquisition_relocs,6u,0x132ff299u,memory) &&
        SudekiMpStoryTargetHash(base,STORY_TARGET_COMMIT,0x95u,NULL,0u,0x45bcfab0u,memory) &&
        SudekiMpStoryTargetHash(base,STORY_TARGET_EXCLUDE,0x38u,NULL,0u,0xf81165adu,memory);
}

__attribute__((naked,noinline,unused)) static void SudekiMpStoryTargetCall(
    void *targeter __attribute__((unused)),void *entry __attribute__((unused))) {
    __asm__ volatile("movl 4(%esp), %eax\n\tjmp *8(%esp)\n\t");
}

/* Prove the existing target slot's intrusive links before native replacement.
 * A null target is valid only with empty links. Native world ownership supplies
 * candidate lifetimes; this predicate does not invent a target entity lease. */
static inline BOOL SudekiMpStoryTargetLinks(uint8_t *slot,SudekiMpStoryTargetMemory memory) {
    if(!memory(slot,12u,TRUE)) return FALSE;
    uint8_t *node=*(uint8_t **)slot,*newer=*(uint8_t **)(slot+4u),*older=*(uint8_t **)(slot+8u);
    if(!node) return !newer && !older;
    if(!memory(node,8u,TRUE)) return FALSE;
    if(newer) {
        if(!memory(newer,12u,TRUE) || *(void **)newer!=node || *(void **)(newer+8u)!=slot) return FALSE;
    } else if(*(void **)(node+4u)!=slot) return FALSE;
    return !older || (memory(older,12u,TRUE) && *(void **)older==node && *(void **)(older+4u)==slot);
}

static inline BOOL SudekiMpStoryTargetOwner(uint8_t *base,uint8_t *actor,void *world,
    uint8_t **targeter,uint8_t **position,uint8_t **arbiter,SudekiMpStoryTargetMemory memory) {
    if(!base || !actor || !world || !memory || !memory(actor,0xb0u,FALSE) ||
        !memory(base+STORY_TARGET_WORLD,4u,FALSE) || *(void **)(base+STORY_TARGET_WORLD)!=world)
        return FALSE;
    /* The observer world and native collision enumerator are distinct owners.
     * Enumeration reads the two collision-system registration vectors. */
    if(!memory(base+STORY_TARGET_COLLISION,4u,FALSE)) return FALSE;
    uint8_t *collision=*(uint8_t **)(base+STORY_TARGET_COLLISION);
    if(!memory(collision,0x8cu,FALSE)) return FALSE;
    for(unsigned i=0;i<2u;++i) {
        uint32_t count=*(uint32_t *)(collision+0x70u+i*0x10u);
        void *items=*(void **)(collision+0x78u+i*0x10u);
        if(count>8192u || (count && !memory(items,(size_t)count*4u,FALSE))) return FALSE;
    }
    *targeter=*(uint8_t **)(actor+0xacu); *position=*(uint8_t **)(actor+0x44u);
    *arbiter=*(uint8_t **)(actor+0x90u);
    return memory(*targeter,0x94u,TRUE) && *(void **)*targeter==base+0x2d4b24u &&
        *(void **)(*targeter+0x10u)==actor &&
        memory(*position,0x100u,TRUE) && *(void **)*position==base+0x2cdefcu &&
        *(void **)(*position+0x10u)==actor &&
        memory(*arbiter,0x64u,FALSE) && *(void **)*arbiter==base+0x2cc9acu &&
        *(void **)(*arbiter+0x10u)==actor;
}

static inline BOOL SudekiMpStoryTargetRefresh(uint8_t *base,void *owner,void *world,
    float x,float z,BOOL *refreshed,SudekiMpStoryTargetMemory memory) {
    uint8_t *actor=owner,*targeter,*position,*arbiter;
    if(refreshed) *refreshed=FALSE;
    if(!refreshed || !isfinite(x) || !isfinite(z) || fabsf(x)>1.0f || fabsf(z)>1.0f ||
        !SudekiMpStoryTargetImageExact(base,memory) ||
        !SudekiMpStoryTargetOwner(base,actor,world,&targeter,&position,&arbiter,memory)) return FALSE;
    /* Respect native disabled auto-target, ranged/first-person targeting, UI,
     * skills, death and spawn ownership. Attacking itself is allowed: target
     * selection must keep receiving input while movement is held (C4 later). */
    if(!(targeter[0x84u]&2u) || (arbiter[0x58u]&0xf0u)==0x20u ||
        (*(uint32_t *)(arbiter+0x50u)&0x0041c410u) || (arbiter[0x60u]&4u)) return TRUE;
    float heading[3]={x,0.0f,z},length=sqrtf(x*x+z*z);
    if(length<0.0001f) {
        /* Neutral/held frames retain the native accepted heading. Feeding the
         * body facing back here would reselect the old enemy while C2's turn
         * step is still catching up. The native controller likewise leaves
         * +48 alone when it does not submit a new movement direction. */
        heading[0]=*(float *)(targeter+0x48u); heading[2]=*(float *)(targeter+0x50u);
        length=sqrtf(heading[0]*heading[0]+heading[2]*heading[2]);
        if(isfinite(length) && length<0.0001f) {
            heading[0]=*(float *)(position+0x50u); heading[2]=*(float *)(position+0x58u);
            length=sqrtf(heading[0]*heading[0]+heading[2]*heading[2]);
        }
    }
    if(!isfinite(length) || length<0.0001f) return FALSE;
    heading[0]/=length; heading[2]/=length;
    int count=*(int *)(targeter+0x88u);
    if(count<0 || count>1024 || (count && !memory(*(void **)(targeter+0x90u),(size_t)count*12u,FALSE)) ||
        !SudekiMpStoryTargetLinks(targeter+0x54u,memory)) return FALSE;
    void *collision=*(void **)(base+STORY_TARGET_COLLISION);
    memcpy(targeter+0x48u,heading,sizeof(heading));
    SudekiMpStoryTargetCall(targeter,base+STORY_TARGET_ACQUIRE);
    *refreshed=TRUE;
    uint8_t *after_targeter,*after_position,*after_arbiter;
    return SudekiMpStoryTargetOwner(base,actor,world,&after_targeter,&after_position,&after_arbiter,memory) &&
        after_targeter==targeter && after_position==position && after_arbiter==arbiter &&
        *(void **)(base+STORY_TARGET_COLLISION)==collision &&
        SudekiMpStoryTargetLinks(targeter+0x54u,memory);
}
#endif
