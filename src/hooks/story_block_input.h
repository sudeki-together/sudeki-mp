#ifndef SUDEKIMP_STORY_BLOCK_INPUT_H
#define SUDEKIMP_STORY_BLOCK_INPUT_H
#include "hooks/story_interaction_guard.h"

/* Named guards for the supported retail block dispatch and its synchronous
 * release request. These ranges contain no relocated absolute operands. */
static inline BOOL SudekiMpStoryBlockImageExact(uint8_t *image,
    SudekiMpStoryInteractionReadable readable) {
    static const struct { const char *name; unsigned rva,size; uint32_t hash; } guards[]={
        {"BlockInputSelection",0xdb189u,0x67u,0x328f01e6u},
        {"BlockStartDispatch",0xdb428u,0xeu,0xa9201dc6u},
        {"BlockRelease",0xdad60u,0x36u,0x54ffe561u},
        {"DodgeAdmissionAndBlockHandoff",0xdae00u,0x71u,0x36e2c008u}
    };
    for(unsigned g=0;g<sizeof(guards)/sizeof(guards[0]);++g) {
        if(!image || !readable || !readable(image+guards[g].rva,guards[g].size)) return FALSE;
        uint32_t hash=2166136261u;
        for(unsigned i=0;i<guards[g].size;++i) hash=(hash^image[guards[g].rva+i])*16777619u;
        if(hash!=guards[g].hash) return FALSE;
    }
    return TRUE;
}
/* Retail DB1EB -> DAD60, EAX=arbiter, plain RET: request native state7
 * only from CBlock states1..4. Native update still owns the ending animation
 * and destruction. This is an input release, not forced task cancellation. */
static inline void SudekiMpStoryBlockRelease(uint8_t *image,void *arbiter) {
    ((void (__attribute__((regparm(1))) *)(void *))(image+0xdad60u))(arbiter);
}
#endif
