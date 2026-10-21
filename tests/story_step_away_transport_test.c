/* Actual loopback UDP; internal socket access only to discard a datagram.
 * No native objects, hooks or game image enter this fixture. */
#include "../src/network/lan_party_session.c"
#include <assert.h>
#include <stdio.h>
static SudekiMpLanPartySession *host,*client;
static SudekiMpLanPartyPresence presence;
static void poll(void) {
    uint32_t now=GetTickCount();
    SudekiMpLanPartyPoll(host,now); SudekiMpLanPartyPoll(client,now);
}
static unsigned drop(SOCKET socket) {
    char bytes[2048]; unsigned n=0;
    while(recv(socket,bytes,sizeof(bytes),0)>0) ++n;
    return n;
}
static void receive_presence(uint32_t ack) {
    SudekiMpLanPartyPresence got; BOOL ok=FALSE;
    for(unsigned i=0;i<400 && !ok;++i) {
        poll(); ok=SudekiMpLanPartyGetPresence(client,&got) && got.ack_request[1]==ack;
        if(!ok) Sleep(1);
    }
    assert(ok && got.ownership.away==presence.ownership.away);
}
static SudekiMpLanPartyCommand request(unsigned id,unsigned away) {
    SudekiMpPartyOwnership *o=&presence.ownership;
    return (SudekiMpLanPartyCommand){.request=id,.world=o->assignment.world,.revision=o->assignment.revision,
        .generation=o->assignment.generation[o->assignment.character[1]],
        .kind=SUDEKIMP_LAN_PARTY_COMMAND_PRESENCE,.player=1,.target=4,.value=away?SUDEKIMP_LAN_PARTY_PRESENCE_AWAY:0};
}
static void exchange(SudekiMpLanPartyCommand c,BOOL lose_request,BOOL lose_ack) {
    assert(SudekiMpLanPartyQueueCommand(client,&c));
    if(lose_request) assert(drop(host->socket)>0);
    SudekiMpLanPartyCommand got; BOOL received=FALSE;
    for(unsigned i=0;i<400 && !received;++i) {
        poll(); received=SudekiMpLanPartyTakeCommand(host,&got); if(!received) Sleep(1);
    }
    assert(received && got.request==c.request && got.value==c.value && got.lease.token);
    assert(!SudekiMpLanPartyTakeCommand(host,&got));
    assert(SudekiMpLanPartyQueueCommand(client,&c)); /* identical outstanding id */
    for(unsigned i=0;i<120;++i) { poll(); Sleep(1); }
    assert(!SudekiMpLanPartyTakeCommand(host,&got)); /* retry cannot execute twice */
    unsigned away=!!(c.value&SUDEKIMP_LAN_PARTY_PRESENCE_AWAY);
    uint32_t revision=presence.ownership.assignment.revision;
    BOOL changed=!!(presence.ownership.away&2)!=away;
    assert(SudekiMpPartyPresenceRequest(&presence.ownership,1,c.request,c.world,c.revision,0,away)==SUDEKIMP_PARTY_SWAP_OK);
    assert(presence.ownership.assignment.revision==revision+(unsigned)changed);
    presence.ack_request[1]=c.request; presence.ack_result[1]=SUDEKIMP_PARTY_SWAP_OK;
    ++presence.sequence; presence.observed_tick=GetTickCount();
    assert(SudekiMpLanPartyPublishPresence(host,&presence));
    if(lose_ack) {
        SudekiMpLanPartyPoll(host,GetTickCount()); assert(drop(client->socket)>0);
        assert(client->peer[1].command_outstanding);
    }
    receive_presence(c.request); assert(!client->peer[1].command_outstanding);
    assert(!SudekiMpLanPartyQueueCommand(client,&c)); /* acknowledged id cannot replay */
    assert(!SudekiMpLanPartyTakeCommand(host,&got));
}
int main(void) {
    SudekiMpLanPartyConfig config={.story_observation=2,.timeout_ms=5000,
        .assignment_enabled=1,.character={3,2,0,1}};
    memset(config.game_hash,0x25,32); host=SudekiMpLanPartyCreate(&config); assert(host);
    config.local_seat=1; config.host_ipv4="127.0.0.1"; config.port=SudekiMpLanPartyPort(host);
    client=SudekiMpLanPartyCreate(&config); assert(client);
    SudekiMpLanPartyPeerStatus h={0},c={0}; BOOL ready=FALSE;
    for(unsigned i=0;i<400 && !ready;++i) {
        poll(); assert(SudekiMpLanPartyPeerStatusGet(host,1,&h)); assert(SudekiMpLanPartyPeerStatusGet(client,1,&c));
        ready=h.phase==SUDEKIMP_LAN_PARTY_OBSERVING && h.transport_confirmed && c.phase==SUDEKIMP_LAN_PARTY_OBSERVING;
        if(!ready) Sleep(2);
    }
    assert(ready);
    SudekiMpPartyAssignment a={.world=1,.revision=1,.generation={1,1,1,1},.humans=3,.available=15,.character={3,2,4,4}};
    assert(SudekiMpPartyOwnershipInitializePresence(&presence.ownership,&a,3,3));
    presence.sequence=1; presence.observed_tick=GetTickCount(); assert(SudekiMpLanPartyPublishPresence(host,&presence));
    receive_presence(0);
    SudekiMpLanPartyCommand invalid=request(1,1); invalid.value|=SUDEKIMP_LAN_PARTY_PRESENCE_MENU;
    assert(!SudekiMpLanPartyQueueCommand(client,&invalid));
    /* Exercise the receive allowlist too: authenticated transport cannot
     * smuggle the MENU bit by bypassing the local queue check. */
    uint8_t body[COMMAND_SIZE]; encode_command(body,&invalid);
    assert(send_peer(client,&client->peer[1],MSG_COMMAND,next_sequence(&client->peer[1]),0,body,sizeof(body)));
    poll(); assert(!SudekiMpLanPartyTakeCommand(host,&invalid));
    exchange(request(1,1),TRUE,TRUE);
    exchange(request(2,1),FALSE,FALSE); /* no-op: no peer-wide revision fence */
    assert(SudekiMpPartyControlDrainComplete(&presence.ownership,1,1,presence.ownership.assignment.generation[2]));
    ++presence.sequence; presence.observed_tick=GetTickCount(); assert(SudekiMpLanPartyPublishPresence(host,&presence));
    exchange(request(3,0),TRUE,TRUE);
    assert(!presence.ownership.away && presence.ownership.control[1]==SUDEKIMP_PARTY_CONTROL_AI);
    SudekiMpLanPartyDestroy(client,FALSE); SudekiMpLanPartyDestroy(host,FALSE);
    puts("step-away loopback request, receive allowlist, duplicates, loss, ACK and no-op passed"); return 0;
}
