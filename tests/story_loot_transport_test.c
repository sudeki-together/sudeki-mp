#include "network/lan_party_session.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static SudekiMpLanPartySession *host,*clients[3];
static SudekiMpLanPartyPeerStatus peers[3];
static SudekiMpLanStoryScene scene={.epoch=1,.revision=1,.phase=SUDEKIMP_LAN_STORY_READY,
    .available_mask=15,.leader_seat=2,.world="loot_fixture"};
static void poll(void) {
    uint32_t now=GetTickCount();
    if(now!=scene.observed_tick) {
        scene.observed_tick=now;
        assert(SudekiMpLanPartyPublishStoryScene(host,&scene));
    }
    SudekiMpLanPartyPoll(host,now);
    for(unsigned i=0;i<3;++i) SudekiMpLanPartyPoll(clients[i],now);
}
static void connected(void) {
    unsigned ready=0;
    for(unsigned t=0;t<400 && ready!=7;++t) {
        poll();
        for(unsigned i=0;i<3;++i) {
            SudekiMpLanPartyPeerStatus remote; SudekiMpLanStoryScene observed;
            assert(SudekiMpLanPartyPeerStatusGet(clients[i],i+1,&peers[i]));
            assert(SudekiMpLanPartyPeerStatusGet(host,i+1,&remote));
            if(peers[i].phase==SUDEKIMP_LAN_PARTY_OBSERVING && remote.transport_confirmed &&
                SudekiMpLanPartyGetStoryScene(clients[i],&peers[i].lease,GetTickCount(),&observed)) ready|=1u<<i;
        }
        if(ready!=7) Sleep(5);
    }
    assert(ready==7);
}
static void converge(const SudekiMpStoryLootState *expected) {
    unsigned got=0;
    for(unsigned t=0;t<150 && got!=7;++t) {
        poll();
        for(unsigned i=0;i<3;++i) {
            SudekiMpStoryLootState actual;
            if(SudekiMpLanPartyGetStoryLoot(clients[i],&peers[i].lease,GetTickCount(),&actual) &&
                actual.revision==expected->revision) {
                assert(!memcmp(&actual,expected,sizeof(actual))); got|=1u<<i;
            }
        }
        if(got!=7) Sleep(5);
    }
    assert(got==7);
}
int main(void) {
    uint8_t save[32]={7},foreign[32]={8};
    SudekiMpLanPartyConfig config={.story_observation=2,.timeout_ms=5000};
    memset(config.game_hash,0x13,sizeof(config.game_hash));
    host=SudekiMpLanPartyCreate(&config); assert(host);
    config.host_ipv4="127.0.0.1"; config.port=SudekiMpLanPartyPort(host);
    for(unsigned i=0;i<3;++i) {
        config.local_seat=(uint8_t)(i+1); clients[i]=SudekiMpLanPartyCreate(&config); assert(clients[i]);
    }
    connected();
    SudekiMpStoryLootState state; SudekiMpStoryLootReceipt receipt;
    assert(SudekiMpStoryLootInitialize(&state,save,365));
    assert(SudekiMpStoryLootBeginVisit(&state,1)==SUDEKIMP_STORY_LOOT_APPLIED);
    for(unsigned i=0;i<60;++i)
        assert(SudekiMpStoryLootRegisterItem(&state,i,99,SUDEKIMP_STORY_LOOT_CONSUMABLE)==SUDEKIMP_STORY_LOOT_APPLIED);
    SudekiMpStoryLootEvent event={.visit=1,.operation=1,.source=77,.kind=SUDEKIMP_STORY_LOOT_BREAK,
        .actor=SUDEKIMP_WALLET_CHARACTER_TAL};
    assert(SudekiMpStoryLootApplyConfirmed(&state,&event,&receipt)==SUDEKIMP_STORY_LOOT_APPLIED);
    event.operation=2; event.kind=SUDEKIMP_STORY_LOOT_WORLD_REWARD; event.source=99; event.amount=10;
    event.line_count=1; event.lines[0]=(SudekiMpStoryLootLine){0,2};
    assert(SudekiMpStoryLootApplyConfirmed(&state,&event,&receipt)==SUDEKIMP_STORY_LOOT_APPLIED);
    assert(!SudekiMpLanPartySendStoryLoot(host,&state,&scene)); /* unbound trusted save */
    assert(SudekiMpLanPartyBindStoryLootSave(host,save));
    assert(!SudekiMpLanPartyBindStoryLootSave(host,foreign));
    for(unsigned i=0;i<3;++i) {
        assert(SudekiMpLanPartyBindStoryLootSave(clients[i],save));
        assert(!SudekiMpLanPartySendStoryLoot(clients[i],&state,&scene)); /* host-only */
    }
    poll(); assert(SudekiMpLanPartySendStoryLoot(host,&state,&scene)); converge(&state);
    SudekiMpStoryLootState old=state;
    event=(SudekiMpStoryLootEvent){.visit=1,.operation=3,.kind=SUDEKIMP_STORY_LOOT_BUY,
        .actor=SUDEKIMP_WALLET_CHARACTER_AILISH,.amount=6,.line_count=1,.lines={{0,1}}};
    assert(SudekiMpStoryLootApplyConfirmed(&state,&event,&receipt)==SUDEKIMP_STORY_LOOT_APPLIED);
    Sleep(20); poll(); assert(SudekiMpLanPartySendStoryLoot(host,&state,&scene)); converge(&state);
    assert(state.money[0]==10 && state.money[1]==4 && state.items[0].quantity[0]==2 && state.items[0].quantity[1]==3);
    Sleep(20); poll(); assert(!SudekiMpLanPartySendStoryLoot(host,&old,&scene));
    old=state; ++old.money[0]; assert(!SudekiMpLanPartySendStoryLoot(host,&old,&scene));
    assert(SudekiMpLanPartySendStoryLoot(host,&state,&scene)); converge(&state); /* absolute retry, no awards */
    SudekiMpLanPartyLease lease=peers[0].lease; ++lease.generation;
    assert(!SudekiMpLanPartyGetStoryLoot(clients[0],&lease,GetTickCount(),&old));
    memset(&old,0x66,sizeof(old)); SudekiMpStoryLootState sentinel=old;
    assert(!SudekiMpLanPartyGetStoryLoot(clients[0],&peers[0].lease,GetTickCount()+1001,&old));
    assert(!memcmp(&old,&sentinel,sizeof(old)));
    /* A reconnect retires presentation, not accounts or the trusted save.
     * This fixture has no native tasks: only transport is being drained. */
    lease=peers[0].lease;
    assert(SudekiMpLanPartyDisconnect(host,&lease));
    for(unsigned t=0;t<150;++t) {
        poll();
        assert(SudekiMpLanPartyPeerStatusGet(clients[0],1,&peers[0]));
        if(peers[0].phase==SUDEKIMP_LAN_PARTY_DRAINING) break;
        Sleep(5);
    }
    assert(peers[0].phase==SUDEKIMP_LAN_PARTY_DRAINING);
    assert(!SudekiMpLanPartyGetStoryLoot(clients[0],&lease,GetTickCount(),&old));
    assert(SudekiMpLanPartyReleaseDrained(host,&lease));
    assert(SudekiMpLanPartyClientRejoin(clients[0]));
    connected();
    assert(peers[0].lease.generation!=lease.generation);
    assert(!SudekiMpLanPartyBindStoryLootSave(clients[0],foreign));
    assert(!SudekiMpLanPartyGetStoryLoot(clients[0],&lease,GetTickCount(),&old));
    assert(!SudekiMpLanPartyGetStoryLoot(clients[0],&peers[0].lease,GetTickCount(),&old));
    Sleep(20); poll(); assert(SudekiMpLanPartySendStoryLoot(host,&state,&scene)); converge(&state);
    assert(state.visit==1 && state.source_count==2 && state.money[1]==4);
    /* A fresh scene invalidates presentation but cannot erase paid sources
     * at the same visit. Only a proven new native visit may retire them. */
    ++scene.epoch; ++scene.revision; Sleep(110); poll();
    for(unsigned i=0;i<3;++i)
        assert(!SudekiMpLanPartyGetStoryLoot(clients[i],&peers[i].lease,GetTickCount(),&old));
    old=state; ++old.revision; old.source_count=0;
    assert(!SudekiMpLanPartySendStoryLoot(host,&old,&scene));
    assert(SudekiMpStoryLootBeginVisit(&state,2)==SUDEKIMP_STORY_LOOT_APPLIED);
    assert(SudekiMpLanPartySendStoryLoot(host,&state,&scene)); converge(&state);
    assert(!state.source_count && state.money[0]==10 && state.money[1]==4);
    for(unsigned i=0;i<3;++i) SudekiMpLanPartyDestroy(clients[i],FALSE);
    SudekiMpLanPartyDestroy(host,FALSE);
    puts("story_loot_transport_test: PASS (host + three UDP clients)"); return 0;
}
