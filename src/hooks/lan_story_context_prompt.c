#include "hooks/lan_story_context_prompt.h"
#include "hooks/lan_story_ally_hud.h"
#include "engine/build_identity.h"
#include "engine/log.h"
#include <stdint.h>
#include <string.h>

enum { RVA_HUD_GLOBAL=0x3c2fc4u, RVA_HUD_UPDATE=0xae430u, HUD_ENABLE_BIT=0x80u };
/* FUN_004ae430 entry: SUB ESP,0x10; PUSH EBX; PUSH EBP; PUSH ESI; MOV ESI,[0x808d94]. */
static const uint8_t update_entry[8]={0x83,0xec,0x10,0x53,0x55,0x56,0x8b,0x35};
static BOOL update_entry_exact(const uint8_t *b) {
    /* The MOV ESI operand is an absolute address relocated with the image. */
    uint8_t expected[12]; uintptr_t operand=(uintptr_t)(b+0x408d94u);
    memcpy(expected,update_entry,sizeof(update_entry)); memcpy(expected+8u,&operand,4u);
    if(!memcmp(b+RVA_HUD_UPDATE,expected,sizeof(expected))) return TRUE;
    /* The ally seat's client HUD hook owns this entry; calling through it runs
     * the native update unchanged whenever the ally is not in melee combat. */
    return SudekiMpLanStoryAllyHudOwnsUpdateEntry(b+RVA_HUD_UPDATE,expected,sizeof(expected));
}
static unsigned logs;

static BOOL readable(const void *p,size_t n) {
    MEMORY_BASIC_INFORMATION m; uintptr_t a=(uintptr_t)p;
    return p && n && VirtualQuery(p,&m,sizeof(m))==sizeof(m) && m.State==MEM_COMMIT &&
        !(m.Protect&(PAGE_GUARD|PAGE_NOACCESS)) && a+n>=a && a+n<=(uintptr_t)m.BaseAddress+m.RegionSize;
}
BOOL SudekiMpLanStoryContextPromptClear(HMODULE game_module) {
    uint8_t *b=(uint8_t *)game_module;
    if(!b || !SudekiMpCheckLoadedExecutable(game_module) || !update_entry_exact(b)) return FALSE;
    uint8_t *hud=*(uint8_t **)(b+RVA_HUD_GLOBAL);
    if(!readable(hud,0x60u)) return FALSE;
    uint32_t flags=*(uint32_t *)(hud+0x58u);
    if(!(flags&~HUD_ENABLE_BIT)) return FALSE;
    *(uint32_t *)(hud+0x58u)=flags&HUD_ENABLE_BIT;
    /* stdcall: one stack argument, callee cleans (SetContextMenuFlags tail-jumps into it). */
    __asm__ volatile("push %[hud]; call *%[fn]" : : [hud]"r"(hud), [fn]"r"(b+RVA_HUD_UPDATE) : "eax","ecx","edx","memory","cc");
    if(logs<20u) { ++logs;
        SudekiMpLogFormat("context_prompt event=cleared flags=%08lx->%08lx mode=%lu\r\n",
            (unsigned long)flags,(unsigned long)*(uint32_t *)(hud+0x58u),(unsigned long)*(uint32_t *)(hud+0x48u)); }
    return TRUE;
}
