#include "engine/story_breakable.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static SudekiMpStoryLootState initial(void) {
    uint8_t save[32]={1}; SudekiMpStoryLootState s;
    assert(SudekiMpStoryLootInitialize(&s,save,365));
    assert(SudekiMpStoryLootBeginVisit(&s,1)==SUDEKIMP_STORY_LOOT_APPLIED);
    return s;
}
static void first_break_wins(void) {
    SudekiMpStoryLootState s=initial(); SudekiMpStoryBreakables b={0};
    SudekiMpStoryBreakableIntent tal,ailish,buki,unused;
    assert(SudekiMpStoryBreakablesBind(&b,&s));
    assert(SudekiMpStoryBreakableRequest(&b,&s,SUDEKIMP_WALLET_CHARACTER_TAL,100,5,10,&tal));
    assert(SudekiMpStoryBreakableRequest(&b,&s,SUDEKIMP_WALLET_CHARACTER_AILISH,100,6,11,&ailish));
    assert(SudekiMpStoryBreakableRequest(&b,&s,SUDEKIMP_WALLET_CHARACTER_BUKI,200,7,12,&buki));
    assert(SudekiMpStoryBreakableStarted(&b,&s,&tal,1000));
    assert(SudekiMpStoryBreakableStarted(&b,&s,&ailish,2000));
    assert(SudekiMpStoryBreakableStarted(&b,&s,&buki,3000));
    uint8_t lost=0xff;
    /* Tal requested first, but Ailish's confirmed break arrives first. */
    assert(SudekiMpStoryBreakableConfirm(&b,&s,100,ailish.actor,ailish.ticket,&lost)==SUDEKIMP_STORY_LOOT_APPLIED);
    assert(lost==3 && b.actors[0].phase==SUDEKIMP_STORY_BREAKABLE_TARGET_GONE &&
        b.actors[1].phase==SUDEKIMP_STORY_BREAKABLE_WON);
    assert(!SudekiMpStoryBreakableCurrent(&b,&s,&tal) && !SudekiMpStoryBreakableCurrent(&b,&s,&ailish));
    assert(SudekiMpStoryBreakableCurrent(&b,&s,&buki)); /* another object never waits */
    assert(b.actors[0].native_pending && b.actors[1].native_pending);
    SudekiMpStoryLootState before=s; SudekiMpStoryBreakables previous=b;
    lost=0xa5;
    assert(SudekiMpStoryBreakableConfirm(&b,&s,100,tal.actor,tal.ticket,&lost)==SUDEKIMP_STORY_LOOT_ALREADY_APPLIED);
    assert(lost==0xa5 && !memcmp(&s,&before,sizeof(s)) && !memcmp(&b,&previous,sizeof(b)));
    assert(!SudekiMpStoryBreakableRequest(&b,&s,SUDEKIMP_WALLET_CHARACTER_ELCO,100,8,13,&unused));
    assert(SudekiMpStoryBreakableConfirm(&b,&s,200,buki.actor,buki.ticket,&lost)==SUDEKIMP_STORY_LOOT_APPLIED);
    assert(lost==4 && s.source_count==2 && !s.money[0]); /* no loot/UI delay or grant */
    assert(!SudekiMpStoryBreakableReturned(&b,tal.ticket,2000));
    assert(SudekiMpStoryBreakableReturned(&b,tal.ticket,1000));
    assert(!SudekiMpStoryBreakableRequest(&b,&s,tal.actor,100,5,10,&unused));
    assert(SudekiMpStoryBreakableRequest(&b,&s,tal.actor,300,5,10,&unused));
    assert(!SudekiMpStoryBreakableReturned(&b,tal.ticket,1000)); /* stale terminal cannot affect new intent */
    assert(SudekiMpStoryBreakableCurrent(&b,&s,&unused));
    assert(SudekiMpStoryLootBeginVisit(&s,2)==SUDEKIMP_STORY_LOOT_APPLIED);
    assert(!SudekiMpStoryBreakablesBind(&b,&s)); /* outstanding exact native lifetimes */
    assert(SudekiMpStoryBreakableReturned(&b,ailish.ticket,2000));
    assert(SudekiMpStoryBreakableReturned(&b,buki.ticket,3000));
    assert(SudekiMpStoryBreakablesBind(&b,&s));
    assert(SudekiMpStoryBreakableRequest(&b,&s,tal.actor,100,9,14,&tal)); /* proved reload, not reconnect */
}
static void replica_and_revocation(void) {
    SudekiMpStoryLootState s=initial(); SudekiMpStoryBreakables b={0};
    SudekiMpStoryBreakableIntent intent;
    assert(SudekiMpStoryBreakablesBind(&b,&s));
    assert(SudekiMpStoryBreakableRequest(&b,&s,SUDEKIMP_WALLET_CHARACTER_TAL,100,5,10,&intent));
    assert(SudekiMpStoryBreakableStarted(&b,&s,&intent,123));
    SudekiMpStoryLootEvent break_event={.visit=1,.operation=1,.source=100,
        .kind=SUDEKIMP_STORY_LOOT_BREAK,.actor=SUDEKIMP_WALLET_CHARACTER_ELCO};
    SudekiMpStoryLootReceipt r;
    assert(SudekiMpStoryLootApplyConfirmed(&s,&break_event,&r)==SUDEKIMP_STORY_LOOT_APPLIED);
    assert(SudekiMpStoryBreakablesReconcile(&b,&s));
    assert(b.actors[0].phase==SUDEKIMP_STORY_BREAKABLE_TARGET_GONE && b.actors[0].native_pending);
    assert(SudekiMpStoryBreakablesBind(&b,&s)); /* repeated bind cannot forget tombstones */
    assert(!SudekiMpStoryBreakableCurrent(&b,&s,&intent));
    assert(SudekiMpStoryBreakableReturned(&b,intent.ticket,123));
    assert(SudekiMpStoryBreakableRequest(&b,&s,intent.actor,200,6,11,&intent));
    SudekiMpStoryBreakableIntent forged=intent; ++forged.actor_generation;
    assert(!SudekiMpStoryBreakableStarted(&b,&s,&forged,124));
    assert(SudekiMpStoryBreakableStarted(&b,&s,&intent,124));
    SudekiMpStoryBreakablesRevokeAll(&b);
    assert(!SudekiMpStoryBreakableCurrent(&b,&s,&intent) && b.actors[0].native_pending);
    assert(!SudekiMpStoryBreakableRequest(&b,&s,intent.actor,300,7,12,&forged));
    assert(SudekiMpStoryBreakableReturned(&b,intent.ticket,124));
    assert(SudekiMpStoryBreakableRequest(&b,&s,intent.actor,300,7,12,&forged));
    SudekiMpStoryLootState old=initial(); assert(!SudekiMpStoryBreakablesReconcile(&b,&old));
    old=s; old.save_identity[0]=2; assert(!SudekiMpStoryBreakablesBind(&b,&old));
    b.next_ticket=UINT64_MAX;
    assert(!SudekiMpStoryBreakableRequest(&b,&s,SUDEKIMP_WALLET_CHARACTER_AILISH,400,8,13,&intent));
}
static void departing_owner(void) {
    SudekiMpStoryLootState s=initial(); SudekiMpStoryBreakables b={0};
    SudekiMpStoryBreakableIntent tal,ailish,fresh;
    assert(SudekiMpStoryBreakablesBind(&b,&s));
    assert(SudekiMpStoryBreakableRequest(&b,&s,SUDEKIMP_WALLET_CHARACTER_TAL,100,5,10,&tal));
    assert(SudekiMpStoryBreakableRequest(&b,&s,SUDEKIMP_WALLET_CHARACTER_AILISH,200,6,11,&ailish));
    assert(SudekiMpStoryBreakableStarted(&b,&s,&tal,123));
    assert(!SudekiMpStoryBreakableRevokeActor(&b,tal.actor,4,10));
    assert(!SudekiMpStoryBreakableRevokeActor(&b,tal.actor,5,9));
    assert(SudekiMpStoryBreakableCurrent(&b,&s,&tal));
    assert(SudekiMpStoryBreakableRevokeActor(&b,tal.actor,5,10));
    assert(!SudekiMpStoryBreakableCurrent(&b,&s,&tal));
    assert(SudekiMpStoryBreakableCurrent(&b,&s,&ailish));
    assert(b.actors[0].native_pending);
    uint8_t mask=0xff;
    SudekiMpStoryLootState before=s;
    assert(SudekiMpStoryBreakableConfirm(&b,&s,100,tal.actor,tal.ticket,&mask)==SUDEKIMP_STORY_LOOT_INVALID);
    assert(!memcmp(&s,&before,sizeof(s)) && mask==0xff);
    assert(!SudekiMpStoryBreakableRequest(&b,&s,tal.actor,100,7,12,&fresh));
    assert(SudekiMpStoryBreakableReturned(&b,tal.ticket,123));
    assert(SudekiMpStoryBreakableRequest(&b,&s,tal.actor,100,7,12,&fresh));
    assert(!SudekiMpStoryBreakableRevokeActor(&b,tal.actor,5,10));
    assert(!SudekiMpStoryBreakableStarted(&b,&s,&tal,124));
    assert(SudekiMpStoryBreakableCurrent(&b,&s,&fresh));
    /* The normal host-native path also invalidates requested (not yet started)
     * client intentions, without inventing a native completion obligation. */
    assert(SudekiMpStoryBreakableConfirm(&b,&s,100,tal.actor,0,&mask)==SUDEKIMP_STORY_LOOT_APPLIED);
    assert(mask==1 && !b.actors[0].native_pending);
    assert(!SudekiMpStoryBreakableCurrent(&b,&s,&fresh));
    assert(SudekiMpStoryBreakableCurrent(&b,&s,&ailish));
}
static void rewards_and_personal_spending(void) {
    SudekiMpStoryLootState s=initial(); SudekiMpStoryBreakables b={0};
    SudekiMpStoryBreakableIntent intents[4];
    const SudekiMpWalletCharacterId actors[4]={SUDEKIMP_WALLET_CHARACTER_TAL,
        SUDEKIMP_WALLET_CHARACTER_AILISH,SUDEKIMP_WALLET_CHARACTER_BUKI,SUDEKIMP_WALLET_CHARACTER_ELCO};
    assert(SudekiMpStoryLootRegisterItem(&s,7,99,SUDEKIMP_STORY_LOOT_CONSUMABLE)==SUDEKIMP_STORY_LOOT_APPLIED);
    assert(SudekiMpStoryLootRegisterItem(&s,9,99,SUDEKIMP_STORY_LOOT_MATERIAL)==SUDEKIMP_STORY_LOOT_APPLIED);
    assert(SudekiMpStoryBreakablesBind(&b,&s));
    for(unsigned c=0;c<4;++c) {
        assert(SudekiMpStoryBreakableRequest(&b,&s,actors[c],100+c,5+c,10+c,&intents[c]));
        assert(SudekiMpStoryBreakableStarted(&b,&s,&intents[c],1000+c));
    }
    /* All four native actions are outstanding together; confirmations may
     * arrive in any order, with no HUD or other actor completion involved. */
    const unsigned order[4]={3,1,0,2}; uint8_t mask;
    for(unsigned i=0;i<4;++i) {
        unsigned c=order[i];
        assert(SudekiMpStoryBreakableConfirm(&b,&s,100+c,actors[c],intents[c].ticket,&mask)==SUDEKIMP_STORY_LOOT_APPLIED);
        assert(mask==(1u<<c));
    }
    SudekiMpStoryLootReceipt receipt;
    SudekiMpStoryLootEvent reward={.visit=1,.operation=s.operation+1,.source=1000,
        .kind=SUDEKIMP_STORY_LOOT_WORLD_REWARD,.actor=actors[3],.amount=10,.line_count=2,
        .lines={{7,1},{9,2}}};
    assert(SudekiMpStoryLootApplyConfirmed(&s,&reward,&receipt)==SUDEKIMP_STORY_LOOT_APPLIED);
    for(unsigned c=0;c<4;++c) {
        assert(s.money[c]==10 && s.items[0].quantity[c]==1 && s.items[1].quantity[c]==2);
        assert(receipt.money_credit[c]==10 && receipt.item_credit[0][c]==1 && receipt.item_credit[1][c]==2);
        assert(b.actors[c].native_pending); /* rewards do not wait for animation drain */
    }
    SudekiMpStoryLootEvent buy={.visit=1,.operation=s.operation+1,.kind=SUDEKIMP_STORY_LOOT_BUY,
        .actor=actors[0],.amount=3,.line_count=1,.lines={{7,1}}};
    assert(SudekiMpStoryLootApplyConfirmed(&s,&buy,&receipt)==SUDEKIMP_STORY_LOOT_APPLIED);
    assert(s.money[0]==7 && s.items[0].quantity[0]==2);
    for(unsigned c=1;c<4;++c) assert(s.money[c]==10 && s.items[0].quantity[c]==1);
    SudekiMpStoryLootState before=s;
    reward.operation=s.operation+1;
    assert(SudekiMpStoryLootApplyConfirmed(&s,&reward,&receipt)==SUDEKIMP_STORY_LOOT_ALREADY_APPLIED);
    assert(!memcmp(&before,&s,sizeof(s)) && s.reserve==365);
}
int main(void) {
    first_break_wins(); replica_and_revocation(); departing_owner(); rewards_and_personal_spending();
    puts("story_breakable_test: PASS (policy only; no native task cancellation)"); return 0;
}
