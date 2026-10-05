#include "engine/story_loot.h"
#include "engine/story_loot_codec.h"
#include "network/lan_story_loot.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static const uint8_t identity[32]={0x29,0x71};
static SudekiMpStoryLootState initial(void) {
    SudekiMpStoryLootState s;
    assert(SudekiMpStoryLootInitialize(&s,identity,365));
    assert(SudekiMpStoryLootBeginVisit(&s,1)==SUDEKIMP_STORY_LOOT_APPLIED);
    assert(SudekiMpStoryLootRegisterItem(&s,7,10,SUDEKIMP_STORY_LOOT_CONSUMABLE)==SUDEKIMP_STORY_LOOT_APPLIED);
    assert(SudekiMpStoryLootRegisterItem(&s,0,99,SUDEKIMP_STORY_LOOT_MATERIAL)==SUDEKIMP_STORY_LOOT_APPLIED);
    return s;
}
static void rejected(SudekiMpStoryLootState *s,SudekiMpStoryLootEvent *e,SudekiMpStoryLootResult want) {
    SudekiMpStoryLootState before=*s,after; memset(&after,0x7b,sizeof(after));
    SudekiMpStoryLootState sentinel=after;
    SudekiMpStoryLootReceipt receipt; memset(&receipt,0x5b,sizeof(receipt));
    SudekiMpStoryLootReceipt original=receipt;
    assert(SudekiMpStoryLootPlan(s,e,&after,&receipt)==want);
    assert(!memcmp(&after,&sentinel,sizeof(after)) && !memcmp(&receipt,&original,sizeof(receipt)));
    assert(SudekiMpStoryLootApplyConfirmed(s,e,&receipt)==want);
    assert(!memcmp(s,&before,sizeof(before)) && !memcmp(&receipt,&original,sizeof(receipt)));
}
static void policy(void) {
    SudekiMpStoryLootState s=initial(),proposal;
    SudekiMpStoryLootReceipt r;
    SudekiMpStoryLootEvent e={.visit=1,.operation=1,.source=77,
        .kind=SUDEKIMP_STORY_LOOT_BREAK,.actor=SUDEKIMP_WALLET_CHARACTER_TAL};
    assert(SudekiMpStoryLootPlan(&s,&e,&proposal,&r)==SUDEKIMP_STORY_LOOT_APPLIED);
    assert(!s.operation && !s.source_count && proposal.source_count==1);
    assert(SudekiMpStoryLootAdvances(&s,&proposal));
    assert(SudekiMpStoryLootApplyConfirmed(&s,&e,&r)==SUDEKIMP_STORY_LOOT_APPLIED);
    rejected(&s,&e,SUDEKIMP_STORY_LOOT_STALE);
    e.operation=2; rejected(&s,&e,SUDEKIMP_STORY_LOOT_ALREADY_APPLIED);
    e.kind=SUDEKIMP_STORY_LOOT_WORLD_REWARD; e.source=100; e.amount=10;
    e.line_count=2; e.lines[0]=(SudekiMpStoryLootLine){0,2}; e.lines[1]=(SudekiMpStoryLootLine){7,2};
    assert(SudekiMpStoryLootApplyConfirmed(&s,&e,&r)==SUDEKIMP_STORY_LOOT_APPLIED);
    for(unsigned c=0;c<4;++c) {
        assert(s.money[c]==10 && s.items[0].quantity[c]==2 && s.items[1].quantity[c]==2);
        assert(r.money_credit[c]==10 && r.item_credit[0][c]==2 && r.item_credit[1][c]==2);
    }
    assert(s.reserve==365); /* No host-native reserve spending or copying. */
    proposal=s; ++proposal.revision; proposal.source_count=0;
    assert(!SudekiMpStoryLootAdvances(&s,&proposal));
    proposal=s; ++proposal.revision; proposal.items[0].quantity[0]=1;
    assert(!SudekiMpStoryLootAdvances(&s,&proposal));
    proposal=s; ++proposal.revision; --proposal.item_count;
    assert(!SudekiMpStoryLootAdvances(&s,&proposal));
    e.operation=3; rejected(&s,&e,SUDEKIMP_STORY_LOOT_ALREADY_APPLIED);
    e.source=101; e.amount=0; e.line_count=1;
    assert(SudekiMpStoryLootApplyConfirmed(&s,&e,&r)==SUDEKIMP_STORY_LOOT_APPLIED);
    for(unsigned c=0;c<4;++c) assert(s.items[0].quantity[c]==4);
    e=(SudekiMpStoryLootEvent){.visit=1,.operation=4,.kind=SUDEKIMP_STORY_LOOT_BUY,
        .actor=SUDEKIMP_WALLET_CHARACTER_AILISH,.amount=6,.line_count=1,.lines={{0,2}}};
    assert(SudekiMpStoryLootApplyConfirmed(&s,&e,&r)==SUDEKIMP_STORY_LOOT_APPLIED);
    assert(s.money[1]==4 && s.items[0].quantity[1]==6);
    for(unsigned c=0;c<4;++c) if(c!=1) assert(s.money[c]==10 && s.items[0].quantity[c]==4);
    e.operation=5; e.kind=SUDEKIMP_STORY_LOOT_SELL; e.amount=3; e.lines[0].quantity=1;
    assert(SudekiMpStoryLootApplyConfirmed(&s,&e,&r)==SUDEKIMP_STORY_LOOT_APPLIED);
    assert(s.money[1]==7 && s.items[0].quantity[1]==5 && s.money[0]==10);
    e.operation=6; e.kind=SUDEKIMP_STORY_LOOT_USE; e.amount=0; e.actor=SUDEKIMP_WALLET_CHARACTER_TAL;
    assert(SudekiMpStoryLootApplyConfirmed(&s,&e,&r)==SUDEKIMP_STORY_LOOT_APPLIED);
    assert(s.items[0].quantity[0]==3 && s.items[0].quantity[1]==5);
    e.operation=7; e.kind=SUDEKIMP_STORY_LOOT_FORGE; e.amount=11;
    rejected(&s,&e,SUDEKIMP_STORY_LOOT_FUNDS);
    e.amount=2; e.lines[0].quantity=4; rejected(&s,&e,SUDEKIMP_STORY_LOOT_OWNERSHIP);
    e.lines[0].quantity=1; assert(SudekiMpStoryLootApplyConfirmed(&s,&e,&r)==SUDEKIMP_STORY_LOOT_APPLIED);
    assert(s.items[0].quantity[0]==2 && s.money[0]==8);
    e.operation=8; e.kind=SUDEKIMP_STORY_LOOT_WORLD_REWARD; e.source=999;
    e.amount=UINT32_MAX; e.lines[0].quantity=UINT32_MAX;
    assert(SudekiMpStoryLootApplyConfirmed(&s,&e,&r)==SUDEKIMP_STORY_LOOT_APPLIED);
    for(unsigned c=0;c<4;++c) assert(s.items[0].quantity[c]==99 && s.money[c]==99999);
    assert(r.item_credit[0][0]==97 && r.item_credit[0][1]==94);
    assert(r.item_overflow[0][0]==UINT32_MAX-97);
    e.operation=9; e.source=1000; e.line_count=2; e.lines[1]=(SudekiMpStoryLootLine){10000,1};
    rejected(&s,&e,SUDEKIMP_STORY_LOOT_INELIGIBLE); /* No partial florin/first-item award. */
    e.lines[1].item=0; rejected(&s,&e,SUDEKIMP_STORY_LOOT_INVALID); /* duplicate line */
    e.line_count=1; e.kind=SUDEKIMP_STORY_LOOT_BUY; e.source=0; e.amount=1; e.lines[0].quantity=1;
    rejected(&s,&e,SUDEKIMP_STORY_LOOT_CAPACITY); /* no charge on a full personal stack */
    e.kind=SUDEKIMP_STORY_LOOT_WORLD_REWARD; e.source=123; e.operation=10;
    rejected(&s,&e,SUDEKIMP_STORY_LOOT_STALE); /* missing operation */
    e.operation=9; e.actor=SUDEKIMP_WALLET_CHARACTER_INVALID;
    rejected(&s,&e,SUDEKIMP_STORY_LOOT_INVALID);
    SudekiMpStoryLootState before=s;
    assert(SudekiMpStoryLootBeginVisit(&s,1)==SUDEKIMP_STORY_LOOT_ALREADY_APPLIED);
    assert(!memcmp(&s,&before,sizeof(s))); /* reconnect is NOT respawn */
    assert(SudekiMpStoryLootBeginVisit(&s,2)==SUDEKIMP_STORY_LOOT_APPLIED);
    assert(SudekiMpStoryLootAdvances(&before,&s));
    assert(s.source_count==0 && s.money[0]==99999 && s.items[0].quantity[0]==99);
    e=(SudekiMpStoryLootEvent){.visit=2,.operation=9,.kind=SUDEKIMP_STORY_LOOT_BREAK,
        .source=77,.actor=SUDEKIMP_WALLET_CHARACTER_ELCO};
    assert(SudekiMpStoryLootApplyConfirmed(&s,&e,&r)==SUDEKIMP_STORY_LOOT_APPLIED);
    e.visit=1; e.operation=10; rejected(&s,&e,SUDEKIMP_STORY_LOOT_STALE);
}
static void capacity(void) {
    SudekiMpStoryLootState s=initial(); SudekiMpStoryLootReceipt r;
    for(unsigned i=0;i<SUDEKIMP_STORY_LOOT_MAX_SOURCES;++i) {
        SudekiMpStoryLootEvent e={.visit=1,.operation=i+1,.source=i+1,
            .kind=SUDEKIMP_STORY_LOOT_BREAK,.actor=SUDEKIMP_WALLET_CHARACTER_TAL};
        assert(SudekiMpStoryLootApplyConfirmed(&s,&e,&r)==SUDEKIMP_STORY_LOOT_APPLIED);
    }
    SudekiMpStoryLootEvent e={.visit=1,.operation=s.operation+1,.source=99999,
        .kind=SUDEKIMP_STORY_LOOT_BREAK,.actor=SUDEKIMP_WALLET_CHARACTER_TAL};
    rejected(&s,&e,SUDEKIMP_STORY_LOOT_CAPACITY); /* Never evict paid sources. */
    e.source=1; rejected(&s,&e,SUDEKIMP_STORY_LOOT_ALREADY_APPLIED);
    s.revision=UINT64_MAX; e.source=99999; rejected(&s,&e,SUDEKIMP_STORY_LOOT_CAPACITY);
}
static void independent_objects(void) {
    SudekiMpStoryLootState s=initial(); SudekiMpStoryLootReceipt r;
    SudekiMpStoryLootEvent e={.visit=1,.operation=1,.source=77,
        .kind=SUDEKIMP_STORY_LOOT_BREAK,.actor=SUDEKIMP_WALLET_CHARACTER_TAL};
    assert(SudekiMpStoryLootApplyConfirmed(&s,&e,&r)==SUDEKIMP_STORY_LOOT_APPLIED);
    e.operation=2; e.source=78; e.actor=SUDEKIMP_WALLET_CHARACTER_AILISH;
    assert(SudekiMpStoryLootApplyConfirmed(&s,&e,&r)==SUDEKIMP_STORY_LOOT_APPLIED);
    assert(s.source_count==2 && !s.money[0]); /* neither break waits for loot/UI */
    /* Separate drops can complete in reverse break order. No global active
     * barrel, native notification completion, or interaction mutex is needed
     * by this account policy. This is not a native concurrency test. */
    e.operation=3; e.kind=SUDEKIMP_STORY_LOOT_WORLD_REWARD; e.source=200;
    e.amount=20; e.line_count=1; e.lines[0]=(SudekiMpStoryLootLine){7,2};
    assert(SudekiMpStoryLootApplyConfirmed(&s,&e,&r)==SUDEKIMP_STORY_LOOT_APPLIED);
    e.operation=4; e.source=201; e.actor=SUDEKIMP_WALLET_CHARACTER_TAL;
    e.amount=10; e.lines[0].quantity=1;
    assert(SudekiMpStoryLootApplyConfirmed(&s,&e,&r)==SUDEKIMP_STORY_LOOT_APPLIED);
    for(unsigned c=0;c<4;++c) assert(s.money[c]==30 && s.items[1].quantity[c]==3);
    e.operation=5; e.actor=SUDEKIMP_WALLET_CHARACTER_BUKI;
    rejected(&s,&e,SUDEKIMP_STORY_LOOT_ALREADY_APPLIED);
    e.source=200; rejected(&s,&e,SUDEKIMP_STORY_LOOT_ALREADY_APPLIED);
    e.kind=SUDEKIMP_STORY_LOOT_BREAK; e.source=77; e.amount=e.line_count=0;
    rejected(&s,&e,SUDEKIMP_STORY_LOOT_ALREADY_APPLIED); /* same object, one winner */
}
static void serialization(void) {
    SudekiMpStoryLootState s=initial(),out,before;
    uint8_t bytes[SUDEKIMP_STORY_LOOT_MAX_SIZE],wrong[32]={2}; size_t n=0;
    assert(SudekiMpStoryLootEncode(&s,bytes,sizeof(bytes),&n));
    memset(&out,0x3b,sizeof(out)); before=out;
    assert(!SudekiMpStoryLootDecode(bytes,n,wrong,&out) && !memcmp(&out,&before,sizeof(out)));
    for(size_t i=0;i<n;++i) {
        bytes[i]^=1;
        assert(!SudekiMpStoryLootDecode(bytes,n,identity,&out));
        assert(!memcmp(&out,&before,sizeof(out))); bytes[i]^=1;
    }
    for(size_t i=0;i<n;++i) assert(!SudekiMpStoryLootDecode(bytes,i,identity,&out));
    assert(SudekiMpStoryLootDecode(bytes,n,identity,&out) && !memcmp(&s,&out,sizeof(s)));
    s.items[0].quantity[0]=100; assert(!SudekiMpStoryLootEncode(&s,bytes,sizeof(bytes),&n));
}
static void fragments(void) {
    SudekiMpStoryLootState s=initial(),out,before; SudekiMpStoryLootReceipt receipt;
    for(unsigned i=0;i<80;++i) {
        SudekiMpStoryLootEvent e={.visit=1,.operation=i+1,.source=i+1,
            .kind=SUDEKIMP_STORY_LOOT_BREAK,.actor=SUDEKIMP_WALLET_CHARACTER_TAL};
        assert(SudekiMpStoryLootApplyConfirmed(&s,&e,&receipt)==SUDEKIMP_STORY_LOOT_APPLIED);
    }
    SudekiMpLanStoryScene scene={.epoch=1,.revision=1,.observed_tick=100,.phase=SUDEKIMP_LAN_STORY_READY,
        .available_mask=15,.leader_seat=2}; strcpy(scene.world,"test_story");
    unsigned count=SudekiMpLanStoryLootChunkCount(&s); assert(count==3);
    SudekiMpLanStoryLootChunk chunks[SUDEKIMP_STORY_LOOT_MAX_CHUNKS];
    for(unsigned i=0;i<count;++i) {
        uint8_t bytes[SUDEKIMP_STORY_LOOT_CHUNK_MAX_SIZE]; size_t n;
        assert(SudekiMpLanStoryLootEncode(&s,&scene,i,bytes,sizeof(bytes),&n));
        assert(SudekiMpLanStoryLootDecode(bytes,n,&chunks[i]));
        bytes[0]^=1; assert(!SudekiMpLanStoryLootDecode(bytes,n,&chunks[i]));
    }
    SudekiMpLanStoryLootAssembly a={0}; memset(&out,0x72,sizeof(out)); before=out;
    assert(SudekiMpLanStoryLootAccept(&a,&chunks[2],10,identity,&out)==1);
    assert(!memcmp(&out,&before,sizeof(out)));
    assert(SudekiMpLanStoryLootAccept(&a,&chunks[2],900,identity,&out)==1 && a.started_at==10);
    SudekiMpLanStoryLootChunk bad=chunks[2]; bad.bytes[0]^=1;
    assert(!SudekiMpLanStoryLootAccept(&a,&bad,901,identity,&out));
    assert(SudekiMpLanStoryLootAccept(&a,&chunks[0],902,identity,&out)==1);
    assert(SudekiMpLanStoryLootAccept(&a,&chunks[1],903,identity,&out)==2 && !memcmp(&out,&s,sizeof(s)));
    assert(SudekiMpLanStoryLootAccept(&a,&chunks[1],904,identity,&out)==1); /* no new completion */
    memset(&a,0,sizeof(a));
    assert(SudekiMpLanStoryLootAccept(&a,&chunks[0],UINT32_MAX-5,identity,&out)==1);
    assert(!SudekiMpLanStoryLootAccept(&a,&chunks[1],1000,identity,&out)); /* wrapped clock, expired */
    for(unsigned i=0;i<count;++i) ++chunks[i].host_tick;
    assert(SudekiMpLanStoryLootAccept(&a,&chunks[1],1001,identity,&out)==1); /* fresh retry */
    bad=chunks[0]; --bad.host_tick; assert(!SudekiMpLanStoryLootAccept(&a,&bad,1001,identity,&out));
    assert(SudekiMpLanStoryLootAccept(&a,&chunks[2],1002,identity,&out)==1);
    assert(SudekiMpLanStoryLootAccept(&a,&chunks[0],1003,identity,&out)==2);
    memset(&a,0,sizeof(a)); chunks[1].bytes[5]^=1;
    assert(SudekiMpLanStoryLootAccept(&a,&chunks[0],10,identity,&out)==1);
    assert(SudekiMpLanStoryLootAccept(&a,&chunks[1],11,identity,&out)==1);
    before=out;
    assert(!SudekiMpLanStoryLootAccept(&a,&chunks[2],12,identity,&out));
    assert(!memcmp(&out,&before,sizeof(out))); /* complete-batch corruption */
}
int main(void) { policy(); capacity(); independent_objects(); serialization(); fragments(); puts("story_loot_test: PASS"); return 0; }
