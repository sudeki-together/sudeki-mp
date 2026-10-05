#ifndef SUDEKIMP_STORY_INTERACTION_GUARD_H
#define SUDEKIMP_STORY_INTERACTION_GUARD_H

#include <windows.h>
#include <stdint.h>
#include <string.h>

typedef BOOL (*SudekiMpStoryInteractionReadable)(const void *,size_t);

/* Exact DB0E0..DB188: the early interaction/advance branches execute BEFORE
 * actor combat validation. Normalize both trigger-singleton operands, and
 * require their actual relocated values as well. No patch or native call. */
static inline BOOL SudekiMpStoryInteractionInputImageExact(uint8_t *image,
    SudekiMpStoryInteractionReadable readable) {
    uint8_t code[0xa9]; uint32_t hash=2166136261u;
    static const unsigned operands[]={0x3cu,0x7bu};
    if(!image || !readable || !readable(image+0xdb0e0u,sizeof(code))) return FALSE;
    memcpy(code,image+0xdb0e0u,sizeof(code));
    for(unsigned i=0;i<2u;++i) {
        uint32_t value,normalized=0x408d24u;
        memcpy(&value,code+operands[i],4u);
        if(value!=(uint32_t)(uintptr_t)(image+normalized)) return FALSE;
        memcpy(code+operands[i],&normalized,4u);
    }
    for(unsigned i=0;i<sizeof(code);++i) hash=(hash^code[i])*16777619u;
    return hash==0x69597b90u;
}

/* Story REMOTE melee press admission only (kind1/2/3, other states zero).
 * Caller must already own a fresh game-thread actor/scene/input lease. This
 * predicate grants no interaction authority, never clears a host latch, and
 * must not be reused for releases, held input, or arbitrary native states.
 *
 * DB0F8 advances an active actor interaction regardless of attack kind.
 * DB11A writes GLOBAL trigger+1D8 from +1D9 bit2 for Weak=1. That latch is
 * subsequently consumed by D6F0/D7A0 for the native front actor, NOT this
 * arbiter's actor. Refuse instead of starting the host's NPC/save/shop action
 * or erasing a pending host press. Strong/Sweep=1 leave that latch alone. */
static inline BOOL SudekiMpStoryMeleeInteractionClear(uint8_t *image,void *actor,
    unsigned kind,SudekiMpStoryInteractionReadable readable) {
    uint8_t *a=actor,*interaction,*mode,*trigger;
    if(!image || !readable || kind<1u || kind>3u || !readable(a,0xacu)) return FALSE;
    interaction=*(uint8_t **)(a+0xa8u);
    if(interaction && (!readable(interaction,0x64u) ||
        *(void **)interaction!=image+0x2d4abcu || *(void **)(interaction+0x10u)!=actor ||
        !readable(mode=*(uint8_t **)(interaction+0x60u),0x4du) ||
        *(void **)mode!=image+0x2cbfccu || mode[0x4cu])) return FALSE;
    if(kind!=1u) return TRUE;
    if(!readable(image+0x408d24u,4u) ||
        !readable(trigger=*(uint8_t **)(image+0x408d24u),0x1dau) ||
        *(void **)trigger!=image+0x2c5458u || *(void **)(trigger+8u)!=image+0x2c5460u ||
        *(void **)(trigger+0x10u)!=image+0x2c5470u) return FALSE;
    return trigger[0x1d8u]==0u && !(trigger[0x1d9u]&2u);
}

#endif
