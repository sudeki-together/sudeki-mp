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
        /* Selection requires a chosen destination (current lobby rule); the
         * test-room destination keeps the host free to pick any character. */
        s->status.start.destination=SUDEKIMP_LOBBY_DEST_TESTROOM;
    }
}
static BOOL hello(SudekiMpLobby *host,SudekiMpLobby *client,unsigned connection,const uint8_t *credential) {
    uint8_t bytes[WIRE]; packet(client,bytes,HELLO);
    name_copy((char *)bytes+BODY,client->player);
    if (credential) memcpy(bytes+BODY+32,credential,16);
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
    assert(SudekiMpLobbyDestination(host,SUDEKIMP_LOBBY_DEST_TESTROOM));
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
    assert(SudekiMpLobbyJoin(client,"127.0.0.1",port,""));
    assert(wait_phase(client,SUDEKIMP_LOBBY_CONNECTED,SUDEKIMP_LOBBY_ADMISSION_NONE));
    /* The lobby UI chooses a destination before anyone selects a character. */
    assert(SudekiMpLobbyDestination(host,SUDEKIMP_LOBBY_DEST_TESTROOM)); Sleep(150);
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
    /* A broken TCP stream retains the client's private credential. */
    AcquireSRWLockExclusive(&client->lock);
    connection_close(&client->connections[0]); error(client,"Simulated transport loss");
    ReleaseSRWLockExclusive(&client->lock);
    DWORD deadline=GetTickCount()+5000;
    do {
        SudekiMpLobbyStatusGet(host,&h); if (!h.members[1].present) break; Sleep(10);
    } while ((LONG)(deadline-GetTickCount())>0);
    assert(!h.members[1].present && h.members[1].reserved && h.members[1].character==1);
    /* Current contract (SudekiMpLobbyReconnect): departure invalidates the
     * credential at once, so a reconnect is a fresh join. It never reclaims
     * the departed slot or its character while that slot drains. */
    assert(SudekiMpLobbyReconnect(client));
    assert(wait_phase(client,SUDEKIMP_LOBBY_CONNECTED,SUDEKIMP_LOBBY_ADMISSION_NONE));
    SudekiMpLobbyStatusGet(client,&c);
    assert(c.local_slot!=1 && c.members[c.local_slot].character==SUDEKIMP_LOBBY_NO_CHARACTER);
    SudekiMpLobbyStatusGet(host,&h);
    assert(h.members[1].reserved && !h.members[1].present && !SudekiMpLobbyHostAdmit(host,1));
    assert(SudekiMpLobbyDestroy(late)); assert(SudekiMpLobbyDestroy(client)); assert(SudekiMpLobbyDestroy(host));
}
/* #72 save transfer fixtures: synthetic records with the fixed native sizes. */
static uint8_t *save_fish,*save_bunny;
static SudekiMpLobbySavedGame transfer_save;
static void make_save_records(void) {
    save_fish=malloc(SUDEKIMP_SAVE_TRANSFER_FISH_BYTES); save_bunny=malloc(SUDEKIMP_SAVE_TRANSFER_BUNNY_BYTES);
    assert(save_fish && save_bunny);
    for (unsigned i=0;i<SUDEKIMP_SAVE_TRANSFER_FISH_BYTES;++i) save_fish[i]=(uint8_t)(i*31u+(i>>9));
    for (unsigned i=0;i<SUDEKIMP_SAVE_TRANSFER_BUNNY_BYTES;++i) save_bunny[i]=(uint8_t)(i^0x5au);
    memset(&transfer_save,0,sizeof(transfer_save));
    transfer_save.folder_slot=11;
    assert(SudekiMpSaveTransferHash(save_fish,SUDEKIMP_SAVE_TRANSFER_FISH_BYTES,transfer_save.fish_sha256));
    assert(SudekiMpSaveTransferHash(save_bunny,SUDEKIMP_SAVE_TRANSFER_BUNNY_BYTES,transfer_save.bunny_sha256));
    strcpy(transfer_save.label,"Transfer fixture");
    transfer_save.party_count=2; transfer_save.leader=3; transfer_save.party_mask=(1u<<3)|(1u<<2);
    transfer_save.party_order[0]=3; transfer_save.party_order[1]=2; transfer_save.party_order[2]=4; transfer_save.party_order[3]=4;
}
static void save_request_packet(SudekiMpLobby *client,uint8_t *bytes,const SudekiMpLobbySavedGame *save) {
    packet(client,bytes,SAVE_REQUEST);
    memcpy(bytes+BODY,save->fish_sha256,32); memcpy(bytes+BODY+32,save->bunny_sha256,32);
}
static void offline_save_transfer(void) {
    SudekiMpLobby host,client; uint8_t bytes[WIRE]; BOOL done=FALSE;
    initialize(&host,TRUE); initialize(&client,FALSE);
    assert(hello(&host,&client,1,NULL)); state(&host,&client,1);
    assert(SudekiMpLobbySelectSavedGame(&host,&transfer_save)); state(&host,&client,1);
    assert(client.status.start.destination==SUDEKIMP_LOBBY_DEST_SAVEDGAME);
    /* Host: only the exact published pair is accepted as the source. */
    save_bunny[3]^=1u;
    assert(!SudekiMpLobbyProvideSaveRecords(&host,save_fish,SUDEKIMP_SAVE_TRANSFER_FISH_BYTES,save_bunny,SUDEKIMP_SAVE_TRANSFER_BUNNY_BYTES));
    save_bunny[3]^=1u;
    assert(!SudekiMpLobbyProvideSaveRecords(&host,save_fish,SUDEKIMP_SAVE_TRANSFER_FISH_BYTES-1u,save_bunny,SUDEKIMP_SAVE_TRANSFER_BUNNY_BYTES));
    assert(SudekiMpLobbyProvideSaveRecords(&host,save_fish,SUDEKIMP_SAVE_TRANSFER_FISH_BYTES,save_bunny,SUDEKIMP_SAVE_TRANSFER_BUNNY_BYTES));
    assert(host.status.save_transfer.host_has_records);
    /* A request for any other identity is ignored, never served. */
    SudekiMpLobbySavedGame other=transfer_save; other.fish_sha256[0]^=1u;
    save_request_packet(&client,bytes,&other); assert(received(&host,1,bytes) && !host.save_tx[1].stream);
    save_request_packet(&client,bytes,&transfer_save); bytes[BODY+64]=1; assert(!received(&host,1,bytes));
    /* Serve a real request; an unsolicited offer is ignored by the client. */
    save_request_packet(&client,bytes,&transfer_save); assert(received(&host,1,bytes) && host.save_tx[1].stream);
    assert(host.status.save_transfer.transfers_started==1u);
    uint8_t offer[WIRE]; packet(&host,offer,SAVE_DATA); assert(SudekiMpSaveTransferSenderOffer(&host.save_tx[1],offer+BODY));
    assert(received(&client,0,offer) && client.status.save_transfer.phase==SUDEKIMP_LOBBY_SAVE_IDLE && !client.save_rx.stream);
    assert(SudekiMpLobbyRequestSave(&client) && client.save_request_pending &&
        client.status.save_transfer.phase==SUDEKIMP_LOBBY_SAVE_REQUESTED);
    assert(received(&client,0,offer) && client.status.save_transfer.phase==SUDEKIMP_LOBBY_SAVE_RECEIVING);
    unsigned chunks=0;
    while (!done) {
        packet(&host,bytes,SAVE_DATA); assert(SudekiMpSaveTransferSenderNext(&host.save_tx[1],bytes+BODY,&done));
        assert(received(&client,0,bytes)); ++chunks;
    }
    assert(chunks==SUDEKIMP_SAVE_TRANSFER_CHUNKS && client.status.save_transfer.phase==SUDEKIMP_LOBBY_SAVE_COMPLETE);
    uint8_t *fish=malloc(SUDEKIMP_SAVE_TRANSFER_FISH_BYTES),bunny[SUDEKIMP_SAVE_TRANSFER_BUNNY_BYTES];
    assert(fish && SudekiMpLobbyTakeReceivedSave(&client,fish,SUDEKIMP_SAVE_TRANSFER_FISH_BYTES,bunny,sizeof(bunny)));
    assert(!memcmp(fish,save_fish,SUDEKIMP_SAVE_TRANSFER_FISH_BYTES) && !memcmp(bunny,save_bunny,sizeof(bunny)));
    assert(client.status.save_transfer.phase==SUDEKIMP_LOBBY_SAVE_IDLE &&
        !SudekiMpLobbyTakeReceivedSave(&client,fish,SUDEKIMP_SAVE_TRANSFER_FISH_BYTES,bunny,sizeof(bunny)));
    /* A tampered chunk is refused at completion; nothing is released. */
    save_request_packet(&client,bytes,&transfer_save); assert(received(&host,1,bytes));
    assert(SudekiMpLobbyRequestSave(&client));
    packet(&host,offer,SAVE_DATA); assert(SudekiMpSaveTransferSenderOffer(&host.save_tx[1],offer+BODY) && received(&client,0,offer));
    done=FALSE; chunks=0;
    while (!done) {
        packet(&host,bytes,SAVE_DATA); assert(SudekiMpSaveTransferSenderNext(&host.save_tx[1],bytes+BODY,&done));
        if (chunks++==100u) bytes[BODY+16+7]^=0x10u;
        assert(received(&client,0,bytes));
    }
    assert(client.status.save_transfer.phase==SUDEKIMP_LOBBY_SAVE_FAILED &&
        client.status.save_transfer.refusal==SUDEKIMP_SAVE_TRANSFER_HASH_MISMATCH &&
        !SudekiMpLobbyTakeReceivedSave(&client,fish,SUDEKIMP_SAVE_TRANSFER_FISH_BYTES,bunny,sizeof(bunny)));
    /* A new destination retires the host pair and its transfers. */
    save_request_packet(&client,bytes,&transfer_save); assert(received(&host,1,bytes) && host.save_tx[1].stream);
    assert(SudekiMpLobbyDestination(&host,SUDEKIMP_LOBBY_DEST_TESTROOM));
    assert(!host.save_records && !host.save_tx[1].stream && !host.status.save_transfer.host_has_records);
    free(fish); save_host_reset(&host); save_client_reset(&client);
}
static void loopback_save_transfer(void) {
    SudekiMpLobby *host=SudekiMpLobbyCreate(),*client=SudekiMpLobbyCreate();
    assert(host && client);
    SOCKET probe=bound_socket(SOCK_STREAM,0,FALSE); struct sockaddr_in endpoint; int size=sizeof(endpoint);
    assert(probe!=INVALID_SOCKET && !getsockname(probe,(struct sockaddr *)&endpoint,&size));
    uint16_t port=ntohs(endpoint.sin_port); close_socket(&probe);
    assert(SudekiMpLobbyHost(host,"Save transfer fixture","",port,FALSE));
    assert(SudekiMpLobbyJoin(client,"127.0.0.1",port,""));
    assert(wait_phase(client,SUDEKIMP_LOBBY_CONNECTED,SUDEKIMP_LOBBY_ADMISSION_NONE));
    assert(SudekiMpLobbySelectSavedGame(host,&transfer_save));
    assert(SudekiMpLobbyProvideSaveRecords(host,save_fish,SUDEKIMP_SAVE_TRANSFER_FISH_BYTES,save_bunny,SUDEKIMP_SAVE_TRANSFER_BUNNY_BYTES));
    SudekiMpLobbyStatus c; DWORD deadline=GetTickCount()+5000;
    do { SudekiMpLobbyStatusGet(client,&c); if (c.start.destination==SUDEKIMP_LOBBY_DEST_SAVEDGAME) break; Sleep(10); }
    while ((LONG)(deadline-GetTickCount())>0);
    assert(c.start.destination==SUDEKIMP_LOBBY_DEST_SAVEDGAME);
    DWORD began=GetTickCount();
    assert(SudekiMpLobbyRequestSave(client));
    deadline=GetTickCount()+30000;
    do { SudekiMpLobbyStatusGet(client,&c); if (c.save_transfer.phase>=SUDEKIMP_LOBBY_SAVE_COMPLETE) break; Sleep(20); }
    while ((LONG)(deadline-GetTickCount())>0);
    assert(c.save_transfer.phase==SUDEKIMP_LOBBY_SAVE_COMPLETE && c.phase==SUDEKIMP_LOBBY_CONNECTED);
    uint8_t *fish=malloc(SUDEKIMP_SAVE_TRANSFER_FISH_BYTES),bunny[SUDEKIMP_SAVE_TRANSFER_BUNNY_BYTES];
    assert(fish && SudekiMpLobbyTakeReceivedSave(client,fish,SUDEKIMP_SAVE_TRANSFER_FISH_BYTES,bunny,sizeof(bunny)));
    assert(!memcmp(fish,save_fish,SUDEKIMP_SAVE_TRANSFER_FISH_BYTES) && !memcmp(bunny,save_bunny,sizeof(bunny)));
    printf("loopback save transfer: %u bytes in %lu ms\n",(unsigned)SUDEKIMP_SAVE_TRANSFER_TOTAL_BYTES,
        (unsigned long)(GetTickCount()-began));
    free(fish);
    assert(SudekiMpLobbyDestroy(client)); assert(SudekiMpLobbyDestroy(host));
}
int main(void) {
    make_save_records(); offline_save_transfer(); loopback_save_transfer();
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
