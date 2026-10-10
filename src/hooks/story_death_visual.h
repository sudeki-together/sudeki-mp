#ifndef SUDEKIMP_STORY_DEATH_VISUAL_H
#define SUDEKIMP_STORY_DEATH_VISUAL_H
#include <windows.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <math.h>

/* Visual-only SFX creation. No CCombat/death/loot entry is called. A portable
 * death style resolves only to the matched resident monster's authored action
 * table; it never supplies an asset name, address, or native flag word. */
typedef BOOL (*SudekiMpStoryVisualMemory)(const void *,size_t,BOOL);
typedef struct SudekiMpStoryDeathVisual {
    void *manager;
    uint32_t resource[3];
    float matrix[16];
} SudekiMpStoryDeathVisual;
_Static_assert(sizeof(void *)==4u && offsetof(SudekiMpStoryDeathVisual,resource)==4u &&
    offsetof(SudekiMpStoryDeathVisual,matrix)==16u,"Death visual wrapper requires the x86 layout");
static const uint8_t story_death_effect_head[]={0xd9,0x44,0x24,0x18,0x56,0x50,0x51,0xd9,0x1c,0x24};
static const uint8_t story_death_effect_return[]={0xc2,0x18,0x00};
static inline BOOL SudekiMpStoryDeathVisualCode(uint8_t *b,SudekiMpStoryVisualMemory m) {
    return b && m(b+0x17da0u,0xacu,FALSE) &&
        !memcmp(b+0x17da0u,story_death_effect_head,sizeof(story_death_effect_head)) &&
        b[0x17e2bu]==0xe8 && b+0x17e30u+*(int32_t *)(b+0x17e2cu)==b+0x18a80u &&
        !memcmp(b+0x17e49u,story_death_effect_return,sizeof(story_death_effect_return));
}
static inline BOOL SudekiMpStoryDeathVisualPrepare(uint8_t *b,uint8_t *e,unsigned style,
    SudekiMpStoryDeathVisual *out,SudekiMpStoryVisualMemory m) {
    if(!out || style<1u || style>4u || !SudekiMpStoryDeathVisualCode(b,m) ||
        !m(e,0xb4u,FALSE) || *(void **)e!=b+0x2d5fa0u ||
        !m(b+0x408d48u,4u,FALSE)) return FALSE;
    uint8_t *manager=*(uint8_t **)(b+0x408d48u),*anim=*(uint8_t **)(e+0x58u);
    unsigned action=style+0x16u;
    if(!m(manager,0x14u,FALSE) || *(void **)manager!=b+0x2c62a0u ||
        !m(anim,0xe0u,FALSE) || *(void **)anim!=b+0x2c8504u || *(void **)(anim+0x10u)!=e) return FALSE;
    uint8_t *info=*(uint8_t **)(anim+0xdcu);
    if(!m(info,0x14u+4u*(action+1u),FALSE) || *(void **)info!=b+0x2d5db4u) return FALSE;
    uint8_t *entry=*(uint8_t **)(info+0x14u+4u*action);
    if(!m(entry,0x5cu,FALSE) || *(void **)entry!=b+0x2d5df0u || entry[0x56u]!=action ||
        (*(uint32_t *)(entry+0x28u)&0x7fu)!=0x29u ||
        !*(uint32_t *)(entry+0x2cu) || *(uint32_t *)(entry+0x2cu)==0x7ffffu) return FALSE;
    /* Attached/weapon effects need their own lifetime contract. Death chunks
     * in the supported ambush are independent world-space effects. */
    if(entry[0x59u]&3u) return FALSE;
    uint8_t *position=*(uint8_t **)(e+0x44u);
    if(!m(position,0xb8u,FALSE) || *(void **)position!=b+0x2cdefcu ||
        *(void **)(position+0x10u)!=e ||
        (*(uintptr_t *)(position+0x94u) && *(uintptr_t *)(position+0x94u)!=4u)) return FALSE;
    uint8_t *wrapper=*(uint8_t **)(position+0xb4u);
    if(!m(wrapper,0x14u,FALSE)) return FALSE;
    uint8_t *object=*(uint8_t **)(wrapper+8u),*renderer=*(uint8_t **)(wrapper+0x10u);
    if(!m(object,0xd0u,FALSE) || !m(renderer,0x14u,FALSE) ||
        *(void **)renderer!=b+0x2df8ecu || *(void **)(object+0x14u)!=renderer ||
        *(void **)(object+0x18u)) return FALSE;
    SudekiMpStoryDeathVisual v={.manager=manager};
    memcpy(v.resource,entry+0x28u,sizeof(v.resource)); v.resource[0]&=0x1fffu;
    uint32_t *record=(uint32_t *)(uintptr_t)v.resource[2];
    if(record && (!m(record,8u,TRUE) || !*record || *record>=0x7fffffffu)) return FALSE;
    memcpy(v.matrix,object+0x90u,sizeof(v.matrix));
    for(unsigned i=0;i<16u;++i) if(!isfinite(v.matrix[i]) || fabsf(v.matrix[i])>1000000.0f) return FALSE;
    *out=v; return TRUE;
}
/* 417DA0: AL=immediate, CL=loop; six stack words (manager, matrix,
 * ResourceName by value, speed), RET 24. It consumes the copied name lease. */
static void * __attribute__((naked,noinline)) SudekiMpStoryDeathVisualCall(
    const SudekiMpStoryDeathVisual *v __attribute__((unused)),void *fn __attribute__((unused))) {
    __asm__ volatile("pushl %ebp\n\tmovl %esp,%ebp\n\tpushl %ebx\n\tpushl %esi\n\tpushl %edi\n\t"
        "movl 8(%ebp),%esi\n\tpushl $0x3f800000\n\tpushl 12(%esi)\n\tpushl 8(%esi)\n\tpushl 4(%esi)\n\t"
        "leal 16(%esi),%eax\n\tpushl %eax\n\tpushl (%esi)\n\txorl %ecx,%ecx\n\tmovl $1,%eax\n\t"
        "call *12(%ebp)\n\tpopl %edi\n\tpopl %esi\n\tpopl %ebx\n\tpopl %ebp\n\tret\n\t");
}
#endif
