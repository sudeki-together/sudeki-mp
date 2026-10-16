/* Pointer-free journal policy plus real loopback UDP. No native inventory or
 * executable is loaded; Complete(applied=TRUE) represents a proved mutation. */
#include "network/lan_party_session.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void policy(void) {
    SudekiMpStoryLootGrant a={1,0,320,1,SUDEKIMP_STORY_LOOT_GRANT_PICKUP,1},back;
    uint8_t bytes[SUDEKIMP_STORY_LOOT_GRANT_WIRE_SIZE];
    assert(SudekiMpStoryLootGrantEncode(&a,0,bytes,sizeof(bytes)));
    assert(SudekiMpStoryLootGrantDecode(bytes,sizeof(bytes),0,&back));
    assert(SudekiMpStoryLootGrantSame(&a,&back));
    assert(!SudekiMpStoryLootGrantDecode(bytes,sizeof(bytes),1,&back));
    assert(!SudekiMpStoryLootGrantDecode(bytes,sizeof(bytes)-1,0,&back));
    bytes[22]=1;assert(!SudekiMpStoryLootGrantDecode(bytes,sizeof(bytes),0,&back));
    for(unsigned source=0;source<8;++source) {
        back=a;back.source=(uint8_t)source;
        assert(!!SudekiMpStoryLootGrantValid(&back)==(source==1 || source==2));
    }
    back=a;back.item=999;assert(!SudekiMpStoryLootGrantValid(&back));
    back=a;back.quantity=0;assert(!SudekiMpStoryLootGrantValid(&back));
    back=a;back.quantity=1000;assert(!SudekiMpStoryLootGrantValid(&back));
    back=a;back.show=2;assert(!SudekiMpStoryLootGrantValid(&back));
    back=a;back.previous=1;assert(!SudekiMpStoryLootGrantValid(&back));
    SudekiMpStoryLootGrantOutbox out={0};SudekiMpStoryLootGrantInbox in={0};
    assert(SudekiMpStoryLootGrantEnqueue(&out,&a));
    assert(SudekiMpStoryLootGrantReceive(&in,&a)==1);
    assert(SudekiMpStoryLootGrantPeek(&in,&back));
    assert(!SudekiMpStoryLootGrantComplete(&in,&a,1)); /* no native claim */
    assert(SudekiMpStoryLootGrantTake(&in,&a));
    assert(!SudekiMpStoryLootGrantTake(&in,&a));
    assert(SudekiMpStoryLootGrantReceive(&in,&a)==1); /* lost ACK/repeated datagram */
    assert(!SudekiMpStoryLootGrantPeek(&in,&back));
    back=a;++back.quantity;assert(!SudekiMpStoryLootGrantReceive(&in,&back));
    assert(SudekiMpStoryLootGrantComplete(&in,&a,1));
    assert(SudekiMpStoryLootGrantReceive(&in,&a)==2);
    assert(!SudekiMpStoryLootGrantTake(&in,&a));
    assert(!SudekiMpStoryLootGrantAcknowledge(&out,&back));
    assert(SudekiMpStoryLootGrantAcknowledge(&out,&a));
    assert(!SudekiMpStoryLootGrantAcknowledge(&out,&a));
    back=a;back.serial=3;back.previous=2;assert(!SudekiMpStoryLootGrantReceive(&in,&back)); /* gap */
    back.previous=1;assert(SudekiMpStoryLootGrantReceive(&in,&back)==1); /* other peers can explain serial gap */
    assert(SudekiMpStoryLootGrantTake(&in,&back));
    assert(!SudekiMpStoryLootGrantComplete(&in,&back,0));
    assert(in.quarantined && !SudekiMpStoryLootGrantReceive(&in,&back));
    /* Finite queue saturates without overwriting, then wraps after ACK. */
    for(unsigned i=0;i<SUDEKIMP_STORY_LOOT_GRANT_CAPACITY;++i) {
        back=(SudekiMpStoryLootGrant){i+2u,i+1u,319,1,SUDEKIMP_STORY_LOOT_GRANT_CHEST,1};
        assert(SudekiMpStoryLootGrantEnqueue(&out,&back));
    }
    SudekiMpStoryLootGrant first=out.records[out.head];
    back.serial++;back.previous++;
    assert(!SudekiMpStoryLootGrantEnqueue(&out,&back));
    assert(out.count==SUDEKIMP_STORY_LOOT_GRANT_CAPACITY && SudekiMpStoryLootGrantSame(&first,&out.records[out.head]));
    assert(SudekiMpStoryLootGrantAcknowledge(&out,&first));
    assert(SudekiMpStoryLootGrantEnqueue(&out,&back));
}

static SudekiMpLanPartySession *host,*client,*other;
static SudekiMpLanPartyPeerStatus peer;
static SudekiMpLanStoryScene scene={.epoch=1,.revision=1,.phase=SUDEKIMP_LAN_STORY_READY,
    .available_mask=15,.leader_seat=3,.world="loot_grant_fixture"};
static void poll(void) {
    uint32_t now=GetTickCount();
    if(now!=scene.observed_tick) {
        scene.observed_tick=now;assert(SudekiMpLanPartyPublishStoryScene(host,&scene));
    }
    SudekiMpLanPartyPoll(host,now);SudekiMpLanPartyPoll(client,now);
    if(other) SudekiMpLanPartyPoll(other,now);
}
static void connected(void) {
    BOOL ready=FALSE;
    for(unsigned i=0;i<400 && !ready;++i) {
        SudekiMpLanPartyPeerStatus server;
        poll();assert(SudekiMpLanPartyPeerStatusGet(client,1,&peer));
        assert(SudekiMpLanPartyPeerStatusGet(host,1,&server));
        ready=peer.phase==SUDEKIMP_LAN_PARTY_OBSERVING && server.transport_confirmed;
        if(!ready)Sleep(2);
    }
    assert(ready);
}
static SudekiMpStoryLootGrant receive(unsigned serial) {
    SudekiMpStoryLootGrant got={0};BOOL ready=FALSE;
    for(unsigned i=0;i<300 && !ready;++i) {
        poll();ready=SudekiMpLanPartyPeekStoryLootGrant(client,&peer.lease,&got);
        if(!ready)Sleep(2);
    }
    assert(ready && got.serial==serial);return got;
}
static void native_once(const SudekiMpStoryLootGrant *g) {
    assert(SudekiMpLanPartyTakeStoryLootGrant(client,&peer.lease,g));
    assert(!SudekiMpLanPartyTakeStoryLootGrant(client,&peer.lease,g));
    assert(SudekiMpLanPartyCompleteStoryLootGrant(client,&peer.lease,g,TRUE));
    assert(!SudekiMpLanPartyCompleteStoryLootGrant(client,&peer.lease,g,TRUE));
}
static void broadcast(void) {
    SudekiMpLanPartyConfig config={.story_observation=2,.timeout_ms=5000};
    memset(config.game_hash,0x62,sizeof(config.game_hash));
    host=SudekiMpLanPartyCreate(&config);assert(host);
    config.local_seat=1;config.host_ipv4="127.0.0.1";config.port=SudekiMpLanPartyPort(host);
    client=SudekiMpLanPartyCreate(&config);assert(client);connected();
    uint32_t serial;
    assert(SudekiMpLanPartyPublishStoryLootGrant(host,1,320,1,1,&serial) && serial==1);
    SudekiMpStoryLootGrant first=receive(1);native_once(&first);
    config.local_seat=2;other=SudekiMpLanPartyCreate(&config);assert(other);
    SudekiMpLanPartyPeerStatus second={0};BOOL ready=FALSE;
    for(unsigned i=0;i<400 && !ready;++i) {
        SudekiMpLanPartyPeerStatus server;
        poll();assert(SudekiMpLanPartyPeerStatusGet(other,2,&second));
        assert(SudekiMpLanPartyPeerStatusGet(host,2,&server));
        ready=second.phase==SUDEKIMP_LAN_PARTY_OBSERVING && server.transport_confirmed;
        if(!ready)Sleep(2);
    }
    assert(ready);
    assert(SudekiMpLanPartyPublishStoryLootGrant(host,2,321,1,1,&serial) && serial==2);
    first=receive(2);assert(first.previous==1);native_once(&first);
    SudekiMpStoryLootGrant second_grant={0};ready=FALSE;
    for(unsigned i=0;i<300 && !ready;++i) {
        poll();ready=SudekiMpLanPartyPeekStoryLootGrant(other,&second.lease,&second_grant);
        if(!ready)Sleep(2);
    }
    assert(ready && second_grant.serial==2 && !second_grant.previous && second_grant.item==321);
    assert(SudekiMpLanPartyTakeStoryLootGrant(other,&second.lease,&second_grant));
    assert(!SudekiMpLanPartyCompleteStoryLootGrant(other,&second.lease,&second_grant,FALSE));
    assert(SudekiMpLanPartyStoryLootGrantFault(other)==SUDEKIMP_STORY_LOOT_GRANT_FAULT_NATIVE);
    assert(!SudekiMpLanPartyTakeStoryLootGrant(other,&second.lease,&second_grant));
    SudekiMpLanPartyDestroy(other,FALSE);other=NULL;
    SudekiMpLanPartyDestroy(client,FALSE);SudekiMpLanPartyDestroy(host,FALSE);
    scene.observed_tick=0;
}
static void transport(void) {
    SudekiMpLanPartyConfig config={.story_observation=2,.timeout_ms=5000};
    memset(config.game_hash,0x62,sizeof(config.game_hash));
    host=SudekiMpLanPartyCreate(&config);assert(host);
    config.local_seat=1;config.host_ipv4="127.0.0.1";config.port=SudekiMpLanPartyPort(host);
    client=SudekiMpLanPartyCreate(&config);assert(client);connected();
    uint32_t serial=0;
    assert(!SudekiMpLanPartyPublishStoryLootGrant(client,1,320,1,1,&serial));
    assert(!SudekiMpLanPartyPublishStoryLootGrant(host,3,320,1,1,&serial)); /* shop/script unrepresentable */
    SudekiMpLanPartyLease recipients[4]={0};recipients[1]=peer.lease;
    assert(!SudekiMpLanPartyPublishStoryLootGrantForRecipients(host,recipients,0,1,320,1,1,&serial));
    ++recipients[1].generation;
    assert(!SudekiMpLanPartyPublishStoryLootGrantForRecipients(host,recipients,2,1,320,1,1,&serial));
    recipients[1]=peer.lease;
    assert(SudekiMpLanPartyPublishStoryLootGrantForRecipients(host,recipients,2,1,320,1,1,&serial) && serial==1);
    assert(SudekiMpLanPartyPublishStoryLootGrant(host,2,321,1,1,&serial) && serial==2);
    SudekiMpStoryLootGrant first=receive(1),got;
    SudekiMpLanPartyLease wrong=peer.lease;++wrong.generation;
    assert(!SudekiMpLanPartyPeekStoryLootGrant(client,&wrong,&got));
    /* Without a native commit the host repeats only the head; the second
     * pickup cannot replace it. A scene transition does not erase either. */
    scene.phase=SUDEKIMP_LAN_STORY_LOADING;scene.available_mask=0;scene.leader_seat=4;++scene.revision;
    scene.observed_tick=GetTickCount();assert(SudekiMpLanPartyPublishStoryScene(host,&scene));
    for(unsigned i=0;i<80;++i) {poll();Sleep(2);}
    assert(SudekiMpLanPartyPeekStoryLootGrant(client,&peer.lease,&got) && SudekiMpStoryLootGrantSame(&got,&first));
    native_once(&first);got=receive(2);assert(got.previous==1 && got.source==2);native_once(&got);
    for(unsigned i=0;i<8;++i) {poll();Sleep(1);}
    assert(!SudekiMpLanPartyPeekStoryLootGrant(client,&peer.lease,&got));
    /* Queue capacity is backpressure, not a destructive overwrite. A retry
     * of the unpublished event is assigned the next serial only after room. */
    for(unsigned i=0;i<SUDEKIMP_STORY_LOOT_GRANT_CAPACITY;++i)
        assert(SudekiMpLanPartyPublishStoryLootGrant(host,1,320,1,1,&serial) && serial==i+3u);
    uint32_t untouched=0xfeedu;
    assert(!SudekiMpLanPartyPublishStoryLootGrant(host,1,320,1,1,&untouched) && untouched==0xfeedu);
    got=receive(3);native_once(&got);
    for(unsigned i=0;i<8;++i) {poll();Sleep(1);}
    assert(SudekiMpLanPartyPublishStoryLootGrant(host,1,320,1,1,&serial) && serial==131u);
    /* Outstanding native outcome is never replayed into a new connection. */
    SudekiMpLanPartyLease old=peer.lease;
    assert(SudekiMpLanPartyDisconnect(host,&peer.lease));
    assert(SudekiMpLanPartyStoryLootGrantFault(host)==SUDEKIMP_STORY_LOOT_GRANT_FAULT_DISCONNECTED);
    for(unsigned i=0;i<8;++i) {poll();Sleep(1);}
    assert(SudekiMpLanPartyReleaseDrained(host,&old));
    assert(SudekiMpLanPartyClientRejoin(client));connected();
    assert(peer.lease.generation!=old.generation);
    assert(!SudekiMpLanPartyTakeStoryLootGrant(client,&old,&got));
    assert(!SudekiMpLanPartyPublishStoryLootGrant(host,1,320,1,1,&serial));
    SudekiMpLanPartyDestroy(client,FALSE);SudekiMpLanPartyDestroy(host,FALSE);
}
int main(void) {
    policy();broadcast();transport();
    puts("story loot grant codec/source/predecessor/dedupe/backpressure/UDP/scene/rejoin quarantine tests passed (no native inventory)");
    return 0;
}
