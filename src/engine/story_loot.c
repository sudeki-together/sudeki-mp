#include "engine/story_loot.h"
#include <string.h>

static int identity_valid(const uint8_t id[32]) {
    unsigned any=0;
    if(!id) return 0;
    for(unsigned i=0;i<32;++i) any|=id[i];
    return any!=0;
}
static int item_kind(uint32_t kind) {
    return kind==SUDEKIMP_STORY_LOOT_CONSUMABLE || kind==SUDEKIMP_STORY_LOOT_MATERIAL;
}
static int source_order(const SudekiMpStoryLootSource *a,const SudekiMpStoryLootSource *b) {
    if(a->kind!=b->kind) return a->kind<b->kind?-1:1;
    return a->id==b->id?0:a->id<b->id?-1:1;
}
int SudekiMpStoryLootValid(const SudekiMpStoryLootState *s) {
    if(!s || !identity_valid(s->save_identity) || !s->revision || s->operation>=s->revision ||
        s->reserve>SUDEKIMP_PERSONAL_WALLET_RESERVE_CAP ||
        s->item_count>SUDEKIMP_STORY_LOOT_MAX_ITEMS ||
        s->source_count>SUDEKIMP_STORY_LOOT_MAX_SOURCES ||
        (!s->visit && (s->operation || s->source_count))) return 0;
    for(unsigned c=0;c<4;++c) if(s->money[c]>SUDEKIMP_PERSONAL_WALLET_BALANCE_CAP) return 0;
    for(unsigned i=0;i<s->item_count;++i) {
        const SudekiMpStoryLootItem *a=&s->items[i];
        if(!a->cap || !item_kind(a->kind) || (i && s->items[i-1].id>=a->id)) return 0;
        for(unsigned c=0;c<4;++c) if(a->quantity[c]>a->cap) return 0;
    }
    for(unsigned i=0;i<s->source_count;++i) {
        const SudekiMpStoryLootSource *a=&s->sources[i];
        if(!a->id || !a->operation || a->operation>s->operation ||
            (a->kind!=SUDEKIMP_STORY_LOOT_BREAK && a->kind!=SUDEKIMP_STORY_LOOT_WORLD_REWARD) ||
            (i && source_order(&s->sources[i-1],a)>=0)) return 0;
        for(unsigned j=0;j<i;++j) if(s->sources[j].operation==a->operation) return 0;
    }
    return 1;
}
int SudekiMpStoryLootAdvances(const SudekiMpStoryLootState *a,const SudekiMpStoryLootState *b) {
    if(!SudekiMpStoryLootValid(a) || !SudekiMpStoryLootValid(b) ||
        memcmp(a->save_identity,b->save_identity,32) || a->reserve!=b->reserve ||
        b->revision<a->revision || b->operation<a->operation || b->visit<a->visit) return 0;
    int unchanged=b->operation==a->operation;
    if(unchanged && memcmp(a->money,b->money,sizeof(a->money))) return 0;
    if(b->revision==a->revision && (b->visit!=a->visit || !unchanged ||
        b->item_count!=a->item_count || b->source_count!=a->source_count)) return 0;
    unsigned prior=0;
    for(unsigned i=0;i<b->item_count;++i) {
        const SudekiMpStoryLootItem *item=&b->items[i];
        if(prior<a->item_count && a->items[prior].id<item->id) return 0;
        if(prior<a->item_count && a->items[prior].id==item->id) {
            const SudekiMpStoryLootItem *old=&a->items[prior++];
            if(old->cap!=item->cap || old->kind!=item->kind ||
                (unchanged && memcmp(old->quantity,item->quantity,sizeof(old->quantity)))) return 0;
        } else if(unchanged) for(unsigned c=0;c<4;++c) if(item->quantity[c]) return 0;
    }
    if(prior!=a->item_count) return 0;
    prior=0;
    for(unsigned i=0;i<b->source_count;++i) {
        const SudekiMpStoryLootSource *source=&b->sources[i];
        if(b->visit==a->visit && prior<a->source_count) {
            int order=source_order(&a->sources[prior],source);
            if(order<0) return 0;
            if(!order) {
                if(a->sources[prior++].operation!=source->operation) return 0;
                continue;
            }
        }
        if(source->operation<=a->operation) return 0;
    }
    return b->visit!=a->visit || prior==a->source_count;
}
int SudekiMpStoryLootInitialize(SudekiMpStoryLootState *s,const uint8_t id[32],uint32_t reserve) {
    if(!s || !identity_valid(id) || reserve>SUDEKIMP_PERSONAL_WALLET_RESERVE_CAP) return 0;
    /* Allow an identity borrowed from the destination itself. */
    uint8_t copy[32]; memcpy(copy,id,32);
    memset(s,0,sizeof(*s)); memcpy(s->save_identity,copy,32);
    s->revision=1; s->reserve=reserve; return 1;
}
SudekiMpStoryLootResult SudekiMpStoryLootBeginVisit(SudekiMpStoryLootState *s,uint64_t visit) {
    if(!SudekiMpStoryLootValid(s) || !visit) return SUDEKIMP_STORY_LOOT_INVALID;
    if(visit==s->visit) return SUDEKIMP_STORY_LOOT_ALREADY_APPLIED;
    if(visit<s->visit) return SUDEKIMP_STORY_LOOT_STALE;
    if(s->revision==UINT64_MAX) return SUDEKIMP_STORY_LOOT_CAPACITY;
    s->visit=visit; ++s->revision; s->source_count=0;
    memset(s->sources,0,sizeof(s->sources));
    return SUDEKIMP_STORY_LOOT_APPLIED;
}
SudekiMpStoryLootResult SudekiMpStoryLootRegisterItem(SudekiMpStoryLootState *s,
    uint32_t id,uint32_t cap,SudekiMpStoryLootItemKind kind) {
    if(!SudekiMpStoryLootValid(s) || !cap || !item_kind((uint32_t)kind))
        return SUDEKIMP_STORY_LOOT_INVALID;
    unsigned at=0;
    while(at<s->item_count && s->items[at].id<id) ++at;
    if(at<s->item_count && s->items[at].id==id)
        return s->items[at].cap==cap && s->items[at].kind==(uint32_t)kind?
            SUDEKIMP_STORY_LOOT_ALREADY_APPLIED:SUDEKIMP_STORY_LOOT_INVALID;
    if(s->item_count==SUDEKIMP_STORY_LOOT_MAX_ITEMS || s->revision==UINT64_MAX)
        return SUDEKIMP_STORY_LOOT_CAPACITY;
    memmove(&s->items[at+1],&s->items[at],(s->item_count-at)*sizeof(s->items[0]));
    s->items[at]=(SudekiMpStoryLootItem){.id=id,.cap=cap,.kind=(uint32_t)kind};
    ++s->item_count; ++s->revision;
    return SUDEKIMP_STORY_LOOT_APPLIED;
}
static int event_valid(const SudekiMpStoryLootEvent *e,unsigned *actor) {
    uint32_t index;
    if(!e || !e->visit || !e->operation ||
        !SudekiMpPersonalWalletCharacterIndex(e->actor,&index) ||
        e->line_count>SUDEKIMP_STORY_LOOT_MAX_LINES) return 0;
    *actor=index;
    for(unsigned i=0;i<e->line_count;++i)
        if(!e->lines[i].quantity || (i && e->lines[i-1].item>=e->lines[i].item)) return 0;
    switch(e->kind) {
    case SUDEKIMP_STORY_LOOT_BREAK: return e->source && !e->amount && !e->line_count;
    case SUDEKIMP_STORY_LOOT_WORLD_REWARD: return e->source && (e->amount || e->line_count);
    case SUDEKIMP_STORY_LOOT_BUY:
    case SUDEKIMP_STORY_LOOT_SELL: return !e->source && e->line_count==1;
    case SUDEKIMP_STORY_LOOT_USE: return !e->source && !e->amount && e->line_count==1;
    case SUDEKIMP_STORY_LOOT_FORGE: return !e->source && (e->amount || e->line_count);
    default: return 0;
    }
}
static uint32_t credit(uint32_t *value,uint32_t cap,uint32_t amount,uint32_t *overflow) {
    uint32_t available=cap-*value,added=amount<available?amount:available;
    *value+=added; *overflow=amount-added; return added;
}
SudekiMpStoryLootResult SudekiMpStoryLootPlan(const SudekiMpStoryLootState *s,
    const SudekiMpStoryLootEvent *e,SudekiMpStoryLootState *after,SudekiMpStoryLootReceipt *receipt) {
    unsigned actor,indices[SUDEKIMP_STORY_LOOT_MAX_LINES];
    if(!after || !receipt || after==s || !SudekiMpStoryLootValid(s) || !event_valid(e,&actor))
        return SUDEKIMP_STORY_LOOT_INVALID;
    if(e->visit!=s->visit || e->operation<=s->operation) return SUDEKIMP_STORY_LOOT_STALE;
    if(s->operation==UINT64_MAX || s->revision==UINT64_MAX) return SUDEKIMP_STORY_LOOT_CAPACITY;
    if(e->operation!=s->operation+1) return SUDEKIMP_STORY_LOOT_STALE;
    SudekiMpStoryLootSource source={.id=e->source,.operation=e->operation,.kind=e->kind};
    unsigned source_at=0;
    if(e->source) {
        while(source_at<s->source_count && source_order(&s->sources[source_at],&source)<0) ++source_at;
        if(source_at<s->source_count && !source_order(&s->sources[source_at],&source))
            return SUDEKIMP_STORY_LOOT_ALREADY_APPLIED;
        if(s->source_count==SUDEKIMP_STORY_LOOT_MAX_SOURCES) return SUDEKIMP_STORY_LOOT_CAPACITY;
    }
    for(unsigned line=0;line<e->line_count;++line) {
        unsigned i=0;
        while(i<s->item_count && s->items[i].id!=e->lines[line].item) ++i;
        if(i==s->item_count) return SUDEKIMP_STORY_LOOT_INELIGIBLE;
        indices[line]=i;
        if(e->kind==SUDEKIMP_STORY_LOOT_BUY && e->lines[line].quantity>s->items[i].cap-s->items[i].quantity[actor])
            return SUDEKIMP_STORY_LOOT_CAPACITY;
        if((e->kind==SUDEKIMP_STORY_LOOT_SELL || e->kind==SUDEKIMP_STORY_LOOT_USE ||
            e->kind==SUDEKIMP_STORY_LOOT_FORGE) && e->lines[line].quantity>s->items[i].quantity[actor])
            return SUDEKIMP_STORY_LOOT_OWNERSHIP;
    }
    if((e->kind==SUDEKIMP_STORY_LOOT_BUY || e->kind==SUDEKIMP_STORY_LOOT_FORGE) && e->amount>s->money[actor])
        return SUDEKIMP_STORY_LOOT_FUNDS;
    /* Everything that can reject was checked before publishing any output. */
    SudekiMpStoryLootState next=*s;
    SudekiMpStoryLootReceipt result={0}; result.operation=e->operation;
    if(e->kind==SUDEKIMP_STORY_LOOT_WORLD_REWARD) {
        for(unsigned c=0;c<4;++c) result.money_credit[c]=credit(&next.money[c],
            SUDEKIMP_PERSONAL_WALLET_BALANCE_CAP,e->amount,&result.money_overflow[c]);
    } else if(e->kind==SUDEKIMP_STORY_LOOT_SELL) {
        result.money_credit[actor]=credit(&next.money[actor],SUDEKIMP_PERSONAL_WALLET_BALANCE_CAP,
            e->amount,&result.money_overflow[actor]);
    } else if(e->kind==SUDEKIMP_STORY_LOOT_BUY || e->kind==SUDEKIMP_STORY_LOOT_FORGE) {
        next.money[actor]-=e->amount; result.money_debit[actor]=e->amount;
    }
    for(unsigned line=0;line<e->line_count;++line) {
        SudekiMpStoryLootItem *item=&next.items[indices[line]];
        uint32_t quantity=e->lines[line].quantity;
        if(e->kind==SUDEKIMP_STORY_LOOT_WORLD_REWARD) {
            for(unsigned c=0;c<4;++c) result.item_credit[line][c]=credit(&item->quantity[c],
                item->cap,quantity,&result.item_overflow[line][c]);
        } else if(e->kind==SUDEKIMP_STORY_LOOT_BUY) {
            item->quantity[actor]+=quantity; result.item_credit[line][actor]=quantity;
        } else {
            item->quantity[actor]-=quantity; result.item_debit[line][actor]=quantity;
        }
    }
    if(e->source) {
        memmove(&next.sources[source_at+1],&next.sources[source_at],
            (next.source_count-source_at)*sizeof(next.sources[0]));
        next.sources[source_at]=source; ++next.source_count;
    }
    ++next.revision; next.operation=e->operation;
    *after=next; *receipt=result;
    return SUDEKIMP_STORY_LOOT_APPLIED;
}
SudekiMpStoryLootResult SudekiMpStoryLootApplyConfirmed(SudekiMpStoryLootState *s,
    const SudekiMpStoryLootEvent *e,SudekiMpStoryLootReceipt *receipt) {
    if(!s || !receipt) return SUDEKIMP_STORY_LOOT_INVALID;
    SudekiMpStoryLootState next;
    SudekiMpStoryLootReceipt result;
    SudekiMpStoryLootResult status=SudekiMpStoryLootPlan(s,e,&next,&result);
    if(status==SUDEKIMP_STORY_LOOT_APPLIED) { *s=next; *receipt=result; }
    return status;
}
