#ifndef SUDEKIMP_LAN_STORY_SHOPKEEPERS_H
#define SUDEKIMP_LAN_STORY_SHOPKEEPERS_H
#include <windows.h>
#include <string.h>
/* H1b table shops (plan D11): each player uses the shopkeeper on their own
 * game, and the shopkeeper's conversation plays only for that player. This
 * is the one shopkeeper identity table: the client's talk prompt and shop
 * (lan_story_shop.c) and the host's speech capture, which keeps the host
 * player's own shopkeeper conversation off every client's screen
 * (lan_story_cinematic.c). Names are native ResourceNames. Pure. */
typedef struct SudekiMpLanStoryShopkeeper { const char *zone,*npc; } SudekiMpLanStoryShopkeeper;
static const SudekiMpLanStoryShopkeeper sudekimp_story_shopkeepers[]={
    {"LNBr_Kamo_shop","NPC_Kamo_NB"},
};
/* The shopkeeper of a table-shop zone, or NULL. */
static inline const SudekiMpLanStoryShopkeeper *SudekiMpLanStoryShopkeeperOf(const char *zone) {
    for(unsigned i=0;zone && i<sizeof(sudekimp_story_shopkeepers)/sizeof(sudekimp_story_shopkeepers[0]);++i)
        if(!_stricmp(sudekimp_story_shopkeepers[i].zone,zone)) return &sudekimp_story_shopkeepers[i];
    return NULL;
}
/* Whole ResourceName match, ignoring case. */
static inline BOOL SudekiMpLanStoryShopkeeperIs(const char *npc) {
    for(unsigned i=0;npc && i<sizeof(sudekimp_story_shopkeepers)/sizeof(sudekimp_story_shopkeepers[0]);++i)
        if(!_stricmp(sudekimp_story_shopkeepers[i].npc,npc)) return TRUE;
    return FALSE;
}
/* Host speech privacy, one step per observed frame. A line spoken by a
 * shopkeeper makes the conversation private; a line by anyone else, or no
 * conversation, makes it public; a conversation frame with no line (the
 * question waiting for its answer) keeps the current state. An unobserved
 * frame is not a step: privacy survives the gap, so a shopkeeper
 * conversation never leaks part-way. */
static inline BOOL SudekiMpLanStorySpeechPrivateStep(BOOL current,BOOL active,BOOL line,BOOL keeper) {
    if(!active) return FALSE;
    if(line) return keeper;
    return current;
}
#endif
