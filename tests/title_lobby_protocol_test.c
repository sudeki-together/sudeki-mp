/* Deterministic packet/policy fixture. No Sudeki image, sockets, or worker.
 * Include the transport implementation to inspect malformed wire input and
 * verify credential/ticket isolation without exporting production internals. */
#include "../src/network/title_lobby.c"
#include <assert.h>
#include <stdio.h>

static void initialize(SudekiMpLobby *s,BOOL host) {
    memset(s,0,sizeof(*s)); InitializeSRWLock(&s->lock);
    s->listener=s->discovery=s->browser=INVALID_SOCKET;
    for (unsigned i=0;i<4;++i) { s->connections[i].socket=INVALID_SOCKET; member_clear(&s->status.members[i]); }
    memset(s->hash,0xa5,sizeof(s->hash));
    s->status.phase=host?SUDEKIMP_LOBBY_HOSTING:SUDEKIMP_LOBBY_CONNECTING;
    s->status.start.revision=s->status.roster_revision=1;
    name_copy(s->status.room,"Test room"); name_copy(s->player,"Player");
    if (host) {
        s->status.members[0].reserved=s->status.members[0].present=1;
        name_copy(s->status.members[0].name,"Host");
        assert(SudekiMpLobbyDestination(s,SUDEKIMP_LOBBY_DEST_TESTROOM));
    }
}
static BOOL hello(SudekiMpLobby *host,SudekiMpLobby *client,unsigned connection,const uint8_t *credential) {
    uint8_t bytes[WIRE]; packet(client,bytes,HELLO);
    name_copy((char *)bytes+BODY,client->player);
    if (credential) memcpy(bytes+BODY+32,credential,16);
    bytes[BODY+48]=client->status.mode;
    return received(host,connection,bytes);
}
static void state(SudekiMpLobby *host,SudekiMpLobby *client,unsigned player) {
    uint8_t bytes[WIRE]; state_packet(host,player,bytes);
    assert(received(client,0,bytes));
}
static void select_request(SudekiMpLobby *host,SudekiMpLobby *client,unsigned connection,unsigned character,BOOL lock) {
    assert(SudekiMpLobbySelectCharacter(client,character,lock));
    assert(received(host,connection,client->command));
    state(host,client,host->connections[connection].player);
    assert(!client->command_pending);
}
static void complete_initial(SudekiMpLobby *host) {
    AcquireSRWLockExclusive(&host->lock);
    for (unsigned i=0;i<4;++i) if (host->status.members[i].present) host->status.members[i].ready=1;
    ReleaseSRWLockExclusive(&host->lock);
    assert(SudekiMpLobbyStartGame(host));
    assert(!SudekiMpLobbyHostRunning(host));
    AcquireSRWLockExclusive(&host->lock);
    assert(apply_ack(host,0,SUDEKIMP_LOBBY_ACK_PREPARED,26780));
    for (unsigned i=1;i<4;++i) if (host->status.start.members&(1u<<i))
        assert(apply_ack(host,i,SUDEKIMP_LOBBY_ACK_PREPARED,0));
    for (unsigned i=0;i<4;++i) if (host->status.start.members&(1u<<i))
        assert(apply_ack(host,i,SUDEKIMP_LOBBY_ACK_LOADED,0));
    ReleaseSRWLockExclusive(&host->lock);
    assert(!SudekiMpLobbyHostRunning(host));
    AcquireSRWLockExclusive(&host->lock);
    for (unsigned i=0;i<4;++i) if (host->status.start.members&(1u<<i))
        assert(apply_ack(host,i,SUDEKIMP_LOBBY_ACK_COMPLETE,0));
    ReleaseSRWLockExclusive(&host->lock);
    assert(SudekiMpLobbyHostRunning(host));
}
static BOOL wait_phase(SudekiMpLobby *s,unsigned phase,unsigned admission_phase) {
    DWORD deadline=GetTickCount()+5000;
    do {
        SudekiMpLobbyStatus status; SudekiMpLobbyStatusGet(s,&status);
        if ((unsigned)status.phase==phase &&
            status.admission[status.local_slot].phase==admission_phase) return TRUE;
        Sleep(10);
    } while ((LONG)(deadline-GetTickCount())>0);
    return FALSE;
}
static void offline_start_reservation(void) {
    SudekiMpLobby host,client;
    initialize(&host,TRUE); initialize(&client,FALSE);
    assert(hello(&host,&client,1,NULL)); state(&host,&client,1);
    assert(SudekiMpLobbySelectCharacter(&host,2,TRUE)); state(&host,&client,1);
    select_request(&host,&client,1,1,TRUE);
    uint8_t credential[16]; memcpy(credential,host.credentials[1],sizeof(credential));
    /* Model the already-tested stream-loss state before the host starts. */
    connection_close(&host.connections[1]);
    host.status.members[1].present=host.status.members[1].ready=0;
    roster_changed(&host);
    complete_initial(&host);
    assert(host.status.start.members==1 && !host.status.start.nonce[1]);
    assert(host.status.members[1].reserved && host.status.members[1].locked &&
        host.status.members[1].character==1 && !memcmp(credential,host.credentials[1],16));
    const uint8_t characters[4]={2,1,4,4};
    assert(SudekiMpLobbyReflectAssignments(&host,characters,3));
    assert(host.status.members[1].character==1 && host.status.members[1].locked);
}
static void loopback(void) {
    SudekiMpLobby *host=SudekiMpLobbyCreate(),*client=SudekiMpLobbyCreate(),*late=SudekiMpLobbyCreate();
    assert(host && client && late);
    SOCKET probe=bound_socket(SOCK_STREAM,0,FALSE); struct sockaddr_in endpoint; int size=sizeof(endpoint);
    assert(probe!=INVALID_SOCKET && !getsockname(probe,(struct sockaddr *)&endpoint,&size));
    uint16_t port=ntohs(endpoint.sin_port); close_socket(&probe);
    assert(SudekiMpLobbyHost(host,"Loopback fixture","",port,FALSE));
    assert(SudekiMpLobbyDestination(host,SUDEKIMP_LOBBY_DEST_TESTROOM));
    assert(SudekiMpLobbyJoin(client,"127.0.0.1",port,""));
    assert(wait_phase(client,SUDEKIMP_LOBBY_CONNECTED,SUDEKIMP_LOBBY_ADMISSION_NONE));
    assert(SudekiMpLobbySelectCharacter(host,2,TRUE));
    /* A concurrent roster change can reject an in-flight choice; the UI retries
     * after observing the newer revision. The fixture waits for that snapshot. */
    Sleep(150);
    assert(SudekiMpLobbySelectCharacter(client,1,TRUE)); Sleep(250);
    SudekiMpLobbyStatus h,c;
    SudekiMpLobbyStatusGet(host,&h); SudekiMpLobbyStatusGet(client,&c);
    assert(h.members[1].locked && c.members[1].locked);
    complete_initial(host);
    assert(wait_phase(client,SUDEKIMP_LOBBY_CONNECTED,SUDEKIMP_LOBBY_ADMISSION_COMPLETE));
    assert(SudekiMpLobbyJoin(late,"127.0.0.1",port,"Late player"));
    assert(wait_phase(late,SUDEKIMP_LOBBY_CONNECTED,SUDEKIMP_LOBBY_ADMISSION_NONE));
    assert(SudekiMpLobbySelectCharacter(late,0,TRUE)); Sleep(250);
    assert(SudekiMpLobbyHostAdmit(host,2));
    assert(wait_phase(late,SUDEKIMP_LOBBY_CONNECTED,SUDEKIMP_LOBBY_ADMISSION_OFFERED));
    SudekiMpLobbyStatusGet(late,&c);
    SudekiMpLobbyAdmission a=c.admission[2];
    SudekiMpLobbyAdmissionAck(late,a.sequence,a.ticket,SUDEKIMP_LOBBY_ACK_PREPARED);
    assert(wait_phase(late,SUDEKIMP_LOBBY_CONNECTED,SUDEKIMP_LOBBY_ADMISSION_PREPARED));
    SudekiMpLobbyAdmissionAck(late,a.sequence,a.ticket,SUDEKIMP_LOBBY_ACK_LOADED);
    assert(wait_phase(late,SUDEKIMP_LOBBY_CONNECTED,SUDEKIMP_LOBBY_ADMISSION_LOADED));
    assert(SudekiMpLobbyHostAdmissionComplete(host,2,a.sequence,a.ticket,TRUE));
    assert(wait_phase(late,SUDEKIMP_LOBBY_CONNECTED,SUDEKIMP_LOBBY_ADMISSION_COMPLETE));
    /* Departure retires the credential. A fresh join may reuse the slot only
     * after the native adapter confirms its reservation has drained. */
    AcquireSRWLockExclusive(&client->lock);
    connection_close(&client->connections[0]); error(client,"Simulated transport loss");
    ReleaseSRWLockExclusive(&client->lock);
    DWORD deadline=GetTickCount()+5000;
    do {
        SudekiMpLobbyStatusGet(host,&h); if (!h.members[1].present) break; Sleep(10);
    } while ((LONG)(deadline-GetTickCount())>0);
    assert(!h.members[1].present && h.members[1].reserved && h.members[1].character==1);
    assert(SudekiMpLobbyReleaseReservation(host,1));
    assert(SudekiMpLobbyReconnect(client));
    assert(wait_phase(client,SUDEKIMP_LOBBY_CONNECTED,SUDEKIMP_LOBBY_ADMISSION_NONE));
    SudekiMpLobbyStatusGet(client,&c); assert(c.local_slot==1 && c.members[1].character==4);
    assert(SudekiMpLobbySelectCharacter(client,1,TRUE)); Sleep(250);
    assert(SudekiMpLobbyHostAdmit(host,1));
    assert(wait_phase(client,SUDEKIMP_LOBBY_CONNECTED,SUDEKIMP_LOBBY_ADMISSION_OFFERED));
    assert(SudekiMpLobbyDestroy(late)); assert(SudekiMpLobbyDestroy(client)); assert(SudekiMpLobbyDestroy(host));
}
static SudekiMpLobbySavedGame saved_fixture(void) {
    SudekiMpLobbySavedGame save={.folder_slot=1,.fish_sha256={1},.bunny_sha256={2},
        .label="Fixture",.party_count=4,.leader=2,.party_mask=15,.party_order={2,3,1,0}};
    return save;
}
static void dev_play(void) {
    SudekiMpLobby host,client,late; uint8_t bytes[WIRE];
    initialize(&host,TRUE); initialize(&client,FALSE); initialize(&late,FALSE);
    assert(!SudekiMpLobbySetMode(&host,SUDEKIMP_LOBBY_MODE_DEV_PLAY));
    host.status.phase=SUDEKIMP_LOBBY_IDLE; client.status.phase=SUDEKIMP_LOBBY_IDLE;
    late.status.phase=SUDEKIMP_LOBBY_IDLE;
    assert(!SudekiMpLobbySetMode(&host,2)); assert(!SudekiMpLobbySetMode(NULL,0));
    assert(SudekiMpLobbySetMode(&host,SUDEKIMP_LOBBY_MODE_DEV_PLAY));
    assert(SudekiMpLobbySetMode(&client,SUDEKIMP_LOBBY_MODE_DEV_PLAY));
    assert(SudekiMpLobbySetMode(&late,SUDEKIMP_LOBBY_MODE_DEV_PLAY));
    host.status.phase=SUDEKIMP_LOBBY_HOSTING; client.status.phase=late.status.phase=SUDEKIMP_LOBBY_CONNECTING;
    SudekiMpLobbySavedGame save=saved_fixture();
    assert(SudekiMpLobbyEnableSavedStart(&host,TRUE));
    assert(SudekiMpLobbyEnableSavedStart(&client,TRUE));
    assert(SudekiMpLobbyEnableSavedStart(&late,TRUE));
    assert(SudekiMpLobbySelectSavedGame(&host,&save));
    assert(!host.status.members[0].locked && host.status.members[0].character==SUDEKIMP_LOBBY_NO_CHARACTER);
    assert(hello(&host,&client,1,NULL)); state(&host,&client,1);
    assert(SudekiMpLobbySelectCharacter(&host,SUDEKIMP_LOBBY_TALOS,TRUE)); state(&host,&client,1);
    select_request(&host,&client,1,SUDEKIMP_LOBBY_TALOS,TRUE);
    assert(!client.status.command_rejected && client.status.members[1].character==SUDEKIMP_LOBBY_TALOS);
    state_packet(&host,1,bytes);
    assert(bytes[4]==12 && bytes[BODY+225]==1 && !bytes[BODY+226] && !bytes[BODY+227]);
    bytes[4]=11; assert(!received(&client,0,bytes));
    for (unsigned i=225;i<=227;++i) {
        state_packet(&host,1,bytes); bytes[BODY+i]=2; assert(!received(&client,0,bytes));
    }
    state_packet(&host,1,bytes); bytes[BODY+225]=255; assert(!received(&client,0,bytes));
    state_packet(&host,1,bytes); bytes[BODY+234+4]=6; assert(!received(&client,0,bytes));
    state_packet(&host,1,bytes); bytes[BODY+234+4]=4; assert(!received(&client,0,bytes));
    state_packet(&host,1,bytes); bytes[BODY+234+8]=5; assert(!received(&client,0,bytes));
    state_packet(&host,1,bytes); bytes[BODY+225]=0; assert(!received(&client,0,bytes));
    assert(strstr(client.status.error,"Lobby mode differs")); state(&host,&client,1);
    assert(!SudekiMpLobbySelectCharacter(&host,6,TRUE));
    assert(SudekiMpLobbySelectCharacter(&host,2,TRUE)); state(&host,&client,1);
    select_request(&host,&client,1,2,TRUE); assert(!client.status.command_rejected);
    assert(SudekiMpLobbySelectCharacter(&host,SUDEKIMP_LOBBY_TALOS,TRUE)); state(&host,&client,1);
    select_request(&host,&client,1,SUDEKIMP_LOBBY_TALOS,TRUE);
    complete_initial(&host); state(&host,&client,1);
    assert(host.status.running && host.status.members[0].character!=save.leader);
    assert(hello(&host,&late,2,NULL)); state(&host,&late,2);
    select_request(&host,&late,2,SUDEKIMP_LOBBY_TALOS,TRUE);
    const uint8_t talos[4]={5,5,4,4}, duplicate[4]={2,2,4,4}, invalid[4]={6,2,4,4};
    assert(SudekiMpLobbyReflectAssignments(&host,talos,3));
    assert(host.status.members[2].character==5 && host.status.members[2].locked);
    assert(SudekiMpLobbyReflectAssignments(&host,duplicate,3));
    assert(!SudekiMpLobbyReflectAssignments(&host,invalid,3));
    assert(SudekiMpLobbyHostAdmit(&host,2)); state(&host,&late,2);
    assert(late.status.admission[2].phase==SUDEKIMP_LOBBY_ADMISSION_OFFERED);
    SudekiMpLobbyLeave(&client); assert(client.status.mode==SUDEKIMP_LOBBY_MODE_DEV_PLAY);
    assert(SudekiMpLobbySetMode(&client,SUDEKIMP_LOBBY_MODE_MULTIPLAYER));
    assert(client.status.mode==SUDEKIMP_LOBBY_MODE_MULTIPLAYER);
}
static void regular_saved_rules(void) {
    SudekiMpLobby host,client; uint8_t bytes[WIRE];
    initialize(&host,TRUE); initialize(&client,FALSE);
    SudekiMpLobbySavedGame save=saved_fixture();
    assert(SudekiMpLobbySelectSavedGame(&host,&save));
    assert(host.status.members[0].character==save.leader && host.status.members[0].locked);
    assert(!SudekiMpLobbySelectCharacter(&host,1,TRUE));
    assert(!SudekiMpLobbySelectCharacter(&host,save.leader,FALSE));
    assert(!SudekiMpLobbySelectCharacter(&host,SUDEKIMP_LOBBY_TALOS,TRUE));
    client.status.mode=SUDEKIMP_LOBBY_MODE_DEV_PLAY;
    assert(!hello(&host,&client,1,NULL)); assert(!host.status.members[1].reserved);
    client.status.mode=SUDEKIMP_LOBBY_MODE_MULTIPLAYER;
    assert(hello(&host,&client,1,NULL)); state(&host,&client,1);
    assert(!SudekiMpLobbySelectCharacter(&client,SUDEKIMP_LOBBY_TALOS,TRUE));
    state_packet(&host,1,bytes); bytes[BODY+234]=5; assert(!received(&client,0,bytes));
    state_packet(&host,1,bytes); bytes[BODY+234]=1; assert(!received(&client,0,bytes));
    packet(&client,bytes,SELECT_CHARACTER); put32(bytes+BODY,1);
    put32(bytes+BODY+4,host.status.roster_revision); bytes[BODY+8]=5; bytes[BODY+9]=1;
    assert(!received(&host,1,bytes));
    for (unsigned i=225;i<=227;++i) {
        state_packet(&host,1,bytes); bytes[BODY+i]=2; assert(!received(&client,0,bytes));
    }
}
static void discovery_modes(void) {
    WSADATA data; assert(!WSAStartup(MAKEWORD(2,2),&data));
    SudekiMpLobby browser; initialize(&browser,FALSE);
    browser.status.mode=SUDEKIMP_LOBBY_MODE_DEV_PLAY;
    browser.browser=bound_socket(SOCK_DGRAM,0,FALSE); assert(browser.browser!=INVALID_SOCKET);
    browser.query_nonce=123;
    struct sockaddr_in address; int size=sizeof(address);
    assert(!getsockname(browser.browser,(struct sockaddr *)&address,&size));
    address.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
    SOCKET sender=bound_socket(SOCK_DGRAM,0,FALSE); assert(sender!=INVALID_SOCKET);
    uint8_t bytes[WIRE]; packet(&browser,bytes,OFFER);
    put64(bytes+BODY,browser.query_nonce); put16(bytes+BODY+8,26770);
    bytes[BODY+10]=1; name_copy((char *)bytes+BODY+11,"Mode fixture");
    put64(bytes+BODY+43,99);
    /* A regular advertisement is ignored from the Dev Play tab. */
    assert(sendto(sender,(const char *)bytes,WIRE,0,(struct sockaddr *)&address,sizeof(address))==WIRE);
    Sleep(10); discovery_poll(&browser,GetTickCount()); assert(!browser.status.server_count);
    bytes[BODY+52]=SUDEKIMP_LOBBY_MODE_DEV_PLAY;
    assert(sendto(sender,(const char *)bytes,WIRE,0,(struct sockaddr *)&address,sizeof(address))==WIRE);
    Sleep(10); discovery_poll(&browser,GetTickCount()); assert(browser.status.server_count==1);
    assert(browser.status.servers[0].mode==SUDEKIMP_LOBBY_MODE_DEV_PLAY);
    browser.status.server_count=0; bytes[BODY+53]=1;
    assert(sendto(sender,(const char *)bytes,WIRE,0,(struct sockaddr *)&address,sizeof(address))==WIRE);
    Sleep(10); discovery_poll(&browser,GetTickCount()); assert(!browser.status.server_count);
    close_socket(&sender); close_socket(&browser.browser); WSACleanup();
}
int main(void) {
    dev_play(); regular_saved_rules(); discovery_modes();
    offline_start_reservation();
    SudekiMpLobby host,client,late,rejoined; uint8_t bytes[WIRE],credential[16];
    initialize(&host,TRUE); initialize(&client,FALSE); initialize(&late,FALSE);
    assert(hello(&host,&client,1,NULL)); state(&host,&client,1);
    assert(client.status.members[1].character==4 && !client.status.members[1].locked);
    SudekiMpLobbyReady(&client,TRUE); assert(!client.desired_ready);
    assert(!SudekiMpLobbyStartGame(&host));
    assert(SudekiMpLobbySelectCharacter(&host,2,TRUE)); state(&host,&client,1);
    select_request(&host,&client,1,2,TRUE);
    assert(client.status.command_rejected && client.status.members[1].character==4);
    select_request(&host,&client,1,1,TRUE);
    assert(!client.status.command_rejected && client.status.members[1].locked);
    uint32_t revision=host.status.roster_revision;
    assert(received(&host,1,client.command)); assert(host.status.roster_revision==revision);
    assert(SudekiMpLobbySetName(&client,""));
    assert(received(&host,1,client.command)); state(&host,&client,1);
    assert(!client.status.members[1].name[0]);
    assert(SudekiMpLobbySelectCharacter(&client,3,TRUE));
    assert(SudekiMpLobbySetName(&host,"Another host"));
    assert(received(&host,1,client.command)); state(&host,&client,1);
    assert(client.status.command_rejected && client.status.members[1].character==1);
    state_packet(&host,1,bytes); bytes[4]=2; assert(!received(&client,0,bytes));
    state_packet(&host,1,bytes); bytes[BODY+233+4]=2; assert(!received(&client,0,bytes));
    state_packet(&host,1,bytes); bytes[BODY+234+4]=2; assert(!received(&client,0,bytes));
    state_packet(&host,1,bytes); bytes[WIRE-1]=1; assert(!received(&client,0,bytes));
    complete_initial(&host); state(&host,&client,1);
    assert(client.status.running && client.status.admission[1].phase==SUDEKIMP_LOBBY_ADMISSION_COMPLETE);
    assert(!SudekiMpLobbySelectCharacter(&host,0,TRUE));
    assert(!SudekiMpLobbySelectCharacter(&client,0,TRUE));
    assert(SudekiMpLobbyHostRuntimeState(&host,SUDEKIMP_LOBBY_SHARED_PAUSE,TRUE));
    state(&host,&client,1); assert(client.status.paused && client.status.absence_policy==SUDEKIMP_LOBBY_SHARED_PAUSE);
    uint64_t generation=host.status.start.generation;
    uint8_t initial_members=host.status.start.members;
    assert(hello(&host,&late,2,NULL)); state(&host,&late,2);
    assert(late.status.running && late.status.admission[2].phase==SUDEKIMP_LOBBY_ADMISSION_NONE);
    assert(!SudekiMpLobbyHostAdmit(&host,2));
    select_request(&host,&late,2,0,TRUE); assert(!late.status.command_rejected);
    const uint8_t confirmed[4]={2,1,4,4}, conflict[4]={0,1,4,4}, duplicate[4]={1,1,4,4};
    assert(SudekiMpLobbyReflectAssignments(&host,confirmed,3));
    assert(host.status.members[2].character==0 && host.status.members[2].locked);
    assert(!SudekiMpLobbyReflectAssignments(&host,duplicate,3));
    assert(SudekiMpLobbyReflectAssignments(&host,conflict,3));
    assert(host.status.members[2].character==4 && !host.status.members[2].locked && host.command_rejected[2]);
    assert(SudekiMpLobbyReflectAssignments(&host,confirmed,3)); state(&host,&late,2);
    select_request(&host,&late,2,0,TRUE); assert(!late.status.command_rejected);
    assert(SudekiMpLobbyHostAdmit(&host,2)); state(&host,&late,2);
    const SudekiMpLobbyAdmission a=late.status.admission[2];
    assert(a.ticket && a.sequence && a.phase==SUDEKIMP_LOBBY_ADMISSION_OFFERED);
    assert(!late.status.admission[0].ticket && !late.status.admission[1].ticket);
    assert(!SudekiMpLobbyHostAdmissionComplete(&host,2,a.sequence,a.ticket,TRUE));
    assert(!admission_ack(&host,2,a.sequence,a.ticket,SUDEKIMP_LOBBY_ACK_LOADED));
    assert(admission_ack(&host,2,a.sequence,a.ticket,SUDEKIMP_LOBBY_ACK_PREPARED));
    assert(admission_ack(&host,2,a.sequence,a.ticket,SUDEKIMP_LOBBY_ACK_LOADED));
    assert(SudekiMpLobbyHostAdmissionComplete(&host,2,a.sequence,a.ticket,TRUE));
    assert(host.status.start.generation==generation && host.status.start.members==initial_members);
    state(&host,&late,2); assert(late.status.admission[2].phase==SUDEKIMP_LOBBY_ADMISSION_COMPLETE);
    memcpy(credential,client.reconnect_credential,16);
    initialize(&rejoined,FALSE); name_copy(rejoined.player,"");
    assert(!hello(&host,&rejoined,3,credential)); /* Connected owner cannot be displaced. */
    host.status.members[1].present=0; host.status.admission[1].phase=SUDEKIMP_LOBBY_ADMISSION_FAILED;
    assert(hello(&host,&rejoined,3,credential)); state(&host,&rejoined,1);
    assert(host.connections[3].player==1 && rejoined.status.members[1].character==1);
    assert(SudekiMpLobbyHostAdmit(&host,1));
    assert(host.status.admission[1].ticket!=host.status.start.nonce[1]);
    assert(!SudekiMpLobbyReleaseReservation(&host,1));
    host.status.members[1].present=0;
    assert(SudekiMpLobbyReleaseReservation(&host,1));
    assert(!host.status.members[1].reserved && zero(host.credentials[1],16));
    host.connections[3].admitted=FALSE;
    assert(!hello(&host,&rejoined,3,credential));
    assert(hello(&host,&rejoined,3,NULL));
    assert(select_character(&host,1,1,TRUE)); /* Reused initial slot has no stale admission. */
    loopback();
    puts("title_lobby_protocol_test: PASS (packet/policy and TCP loopback; no native gameplay)");
    return 0;
}
