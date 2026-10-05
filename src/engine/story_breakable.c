#include "engine/story_breakable.h"
#include <string.h>

static int open_phase(uint32_t phase) {
    return phase==SUDEKIMP_STORY_BREAKABLE_REQUESTED || phase==SUDEKIMP_STORY_BREAKABLE_ACTIVE;
}
static int bound(const SudekiMpStoryBreakables *b,const SudekiMpStoryLootState *s) {
    return b && SudekiMpStoryLootValid(s) && b->visit && b->visit==s->visit &&
        !memcmp(b->save_identity,s->save_identity,32) && s->revision>=b->revision_floor;
}
static int same(const SudekiMpStoryBreakableIntent *a,const SudekiMpStoryBreakableIntent *b) {
    return a && b && a->ticket && a->ticket==b->ticket && a->source==b->source &&
        a->actor==b->actor && a->actor_generation==b->actor_generation &&
        a->connection_generation==b->connection_generation;
}
int SudekiMpStoryBreakableConsumed(const SudekiMpStoryLootState *s,uint64_t source) {
    if(!source || !SudekiMpStoryLootValid(s)) return 0;
    for(unsigned i=0;i<s->source_count;++i)
        if(s->sources[i].kind==SUDEKIMP_STORY_LOOT_BREAK && s->sources[i].id==source) return 1;
    return 0;
}
int SudekiMpStoryBreakablesBind(SudekiMpStoryBreakables *b,const SudekiMpStoryLootState *s) {
    if(!b || !SudekiMpStoryLootValid(s) || !s->visit) return 0;
    if(b->visit) {
        if(memcmp(b->save_identity,s->save_identity,32) || s->visit<b->visit ||
            s->revision<b->revision_floor) return 0;
        if(s->visit==b->visit) return SudekiMpStoryBreakablesReconcile(b,s);
        for(unsigned c=0;c<4;++c) if(b->actors[c].native_pending) return 0;
    }
    memcpy(b->save_identity,s->save_identity,32);
    b->visit=s->visit; b->revision_floor=s->revision;
    memset(b->actors,0,sizeof(b->actors));
    return 1;
}
int SudekiMpStoryBreakablesReconcile(SudekiMpStoryBreakables *b,const SudekiMpStoryLootState *s) {
    if(!bound(b,s)) return 0;
    for(unsigned c=0;c<4;++c) {
        SudekiMpStoryBreakableIntent *intent=&b->actors[c];
        if(open_phase(intent->phase) && SudekiMpStoryBreakableConsumed(s,intent->source))
            intent->phase=SUDEKIMP_STORY_BREAKABLE_TARGET_GONE;
    }
    b->revision_floor=s->revision;
    return 1;
}
int SudekiMpStoryBreakableRequest(SudekiMpStoryBreakables *b,const SudekiMpStoryLootState *s,
    SudekiMpWalletCharacterId actor,uint64_t source,uint64_t generation,uint64_t connection,
    SudekiMpStoryBreakableIntent *out) {
    uint32_t c;
    if(!out || !bound(b,s) || !source || !generation || !connection ||
        !SudekiMpPersonalWalletCharacterIndex(actor,&c) || b->next_ticket==UINT64_MAX ||
        SudekiMpStoryBreakableConsumed(s,source) || s->source_count==SUDEKIMP_STORY_LOOT_MAX_SOURCES ||
        s->revision==UINT64_MAX || b->actors[c].native_pending || open_phase(b->actors[c].phase)) return 0;
    /* No scan for another actor using this source: request time is NOT the
     * winning break, and a different source is never globally blocked. */
    SudekiMpStoryBreakableIntent intent={.ticket=b->next_ticket+1,.source=source,
        .actor_generation=generation,.connection_generation=connection,.actor=actor,
        .phase=SUDEKIMP_STORY_BREAKABLE_REQUESTED};
    b->next_ticket=intent.ticket; b->revision_floor=s->revision;
    b->actors[c]=intent; *out=intent; return 1;
}
int SudekiMpStoryBreakableCurrent(const SudekiMpStoryBreakables *b,const SudekiMpStoryLootState *s,
    const SudekiMpStoryBreakableIntent *intent) {
    uint32_t c;
    return intent && bound(b,s) && SudekiMpPersonalWalletCharacterIndex(intent->actor,&c) &&
        same(&b->actors[c],intent) && open_phase(b->actors[c].phase) &&
        !SudekiMpStoryBreakableConsumed(s,intent->source);
}
int SudekiMpStoryBreakableStarted(SudekiMpStoryBreakables *b,const SudekiMpStoryLootState *s,
    const SudekiMpStoryBreakableIntent *intent,uint64_t task) {
    uint32_t c;
    if(!task || !SudekiMpStoryBreakableCurrent(b,s,intent) ||
        !SudekiMpPersonalWalletCharacterIndex(intent->actor,&c)) return 0;
    SudekiMpStoryBreakableIntent *current=&b->actors[c];
    if(current->phase!=SUDEKIMP_STORY_BREAKABLE_REQUESTED || current->native_pending) return 0;
    current->native_task=task; current->native_pending=1;
    current->phase=SUDEKIMP_STORY_BREAKABLE_ACTIVE; return 1;
}
SudekiMpStoryLootResult SudekiMpStoryBreakableConfirm(SudekiMpStoryBreakables *b,
    SudekiMpStoryLootState *s,uint64_t source,SudekiMpWalletCharacterId winner,
    uint64_t ticket,uint8_t *invalidated) {
    uint32_t c;
    if(!invalidated || !bound(b,s) || !source ||
        !SudekiMpPersonalWalletCharacterIndex(winner,&c)) return SUDEKIMP_STORY_LOOT_INVALID;
    if(SudekiMpStoryBreakableConsumed(s,source)) return SUDEKIMP_STORY_LOOT_ALREADY_APPLIED;
    if(ticket && (b->actors[c].ticket!=ticket || b->actors[c].source!=source ||
        b->actors[c].phase!=SUDEKIMP_STORY_BREAKABLE_ACTIVE || !b->actors[c].native_pending))
        return SUDEKIMP_STORY_LOOT_INVALID;
    if(s->operation==UINT64_MAX) return SUDEKIMP_STORY_LOOT_CAPACITY;
    SudekiMpStoryLootEvent event={.visit=s->visit,.operation=s->operation+1,.source=source,
        .kind=SUDEKIMP_STORY_LOOT_BREAK,.actor=winner};
    SudekiMpStoryLootReceipt receipt;
    SudekiMpStoryLootResult result=SudekiMpStoryLootApplyConfirmed(s,&event,&receipt);
    if(result!=SUDEKIMP_STORY_LOOT_APPLIED) return result;
    uint8_t mask=0;
    for(unsigned i=0;i<4;++i) {
        SudekiMpStoryBreakableIntent *intent=&b->actors[i];
        if(intent->source!=source || !open_phase(intent->phase)) continue;
        mask|=(uint8_t)(1u<<i);
        intent->phase=ticket && i==c && intent->ticket==ticket?
            SUDEKIMP_STORY_BREAKABLE_WON:SUDEKIMP_STORY_BREAKABLE_TARGET_GONE;
        /* Preserve native_task/native_pending for both winner and losers. */
    }
    b->revision_floor=s->revision; *invalidated=mask;
    return SUDEKIMP_STORY_LOOT_APPLIED;
}
int SudekiMpStoryBreakableRevokeActor(SudekiMpStoryBreakables *b,SudekiMpWalletCharacterId actor,
    uint64_t generation,uint64_t connection) {
    uint32_t c;
    if(!b || !generation || !connection || !SudekiMpPersonalWalletCharacterIndex(actor,&c)) return 0;
    SudekiMpStoryBreakableIntent *intent=&b->actors[c];
    if(intent->actor_generation!=generation || intent->connection_generation!=connection ||
        !open_phase(intent->phase)) return 0;
    intent->phase=SUDEKIMP_STORY_BREAKABLE_REVOKED; return 1;
}
void SudekiMpStoryBreakablesRevokeAll(SudekiMpStoryBreakables *b) {
    if(!b) return;
    for(unsigned c=0;c<4;++c) if(open_phase(b->actors[c].phase))
        b->actors[c].phase=SUDEKIMP_STORY_BREAKABLE_REVOKED;
}
int SudekiMpStoryBreakableReturned(SudekiMpStoryBreakables *b,uint64_t ticket,uint64_t task) {
    if(!b || !ticket || !task) return 0;
    for(unsigned c=0;c<4;++c) {
        SudekiMpStoryBreakableIntent *intent=&b->actors[c];
        if(intent->ticket!=ticket || intent->native_task!=task || !intent->native_pending) continue;
        intent->native_pending=0;
        if(open_phase(intent->phase)) intent->phase=SUDEKIMP_STORY_BREAKABLE_REVOKED;
        return 1;
    }
    return 0;
}
