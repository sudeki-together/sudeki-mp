/* Real loopback plus adversarial envelopes through the actual receiver. */
#include "../src/network/lan_party_session.c"
#include <assert.h>
#include <stdio.h>
static SudekiMpLanPartySession *host,*client;
static SudekiMpLanStoryScene scene={.epoch=7,.revision=9,.phase=SUDEKIMP_LAN_STORY_READY,
    .available_mask=12,.leader_seat=3,.world="newbrightwater"};
static void pump(unsigned n) {
    for(unsigned i=0;i<n;++i) {uint32_t now=GetTickCount();
        SudekiMpLanPartyPoll(host,now);SudekiMpLanPartyPoll(client,now);Sleep(2);}
}
static void start_pair(void) {
    SudekiMpLanPartyConfig config={.story_observation=2,.timeout_ms=5000};
    memset(config.game_hash,0x62,32);host=SudekiMpLanPartyCreate(&config);assert(host);
    config.local_seat=1;config.host_ipv4="127.0.0.1";config.port=SudekiMpLanPartyPort(host);
    client=SudekiMpLanPartyCreate(&config);assert(client);
    for(unsigned i=0;i<400 && !host->peer[1].movie_ready;++i)pump(1);
    assert(host->peer[1].movie_ready && host->peer[1].status.transport_confirmed);
    scene.observed_tick=GetTickCount();assert(SudekiMpLanPartyPublishStoryScene(host,&scene));pump(3);
}
static SudekiMpStoryMovie get_event(unsigned active) {
    SudekiMpStoryMovie event={0};SudekiMpLanPartyLease lease;
    for(unsigned i=0;i<200;++i) {
        pump(1);
        if(SudekiMpLanPartyMovieGet(client,GetTickCount(),&lease,&event) && event.active==active) return event;
    }
    assert(!"movie event missing");return event;
}
static void capability_confirms(void) {
    start_pair();
    SudekiMpLanPartyPoll(host,GetTickCount());
    host->peer[1].status.transport_confirmed=0;
    Peer *p=&client->peer[1];
    assert(send_peer(client,p,MSG_STORY_MOVIE_READY,next_sequence(p),0,NULL,0));
    SudekiMpLanPartyPoll(host,GetTickCount());
    assert(host->peer[1].status.transport_confirmed);
    SudekiMpLanPartyDestroy(client,FALSE);SudekiMpLanPartyDestroy(host,FALSE);
}
static void codecs(void) {
    SudekiMpStoryMovie a={1,7,9,0,1,1},b={0};uint8_t bytes[20];
    assert(SudekiMpStoryMovieEncode(&a,bytes,20) && SudekiMpStoryMovieDecode(bytes,20,&b));
    assert(SudekiMpStoryMovieSame(&a,&b));
    for(unsigned field=0;field<8;++field) {
        assert(SudekiMpStoryMovieEncode(&a,bytes,20));
        if(field==0)bytes[0]=2;
        if(field==1)bytes[1]=2;
        if(field==2)bytes[2]=2;
        if(field==3)bytes[3]=1;
        if(field>=4 && field<=6)memset(bytes+4+4*(field-4),0,4);
        if(field==7)memset(bytes+16,0xff,4);
        assert(!SudekiMpStoryMovieDecode(bytes,20,&b));
    }
    assert(!SudekiMpStoryMovieDecode(bytes,19,&b));
    b=a;b.active=0;assert(SudekiMpStoryMovieAdvances(&a,&b));
    assert(!SudekiMpStoryMovieAdvances(&b,&a));
    b=a;b.revision++;assert(!SudekiMpStoryMovieAdvances(&a,&b));
    b=a;b.serial=2;assert(SudekiMpStoryMovieAdvances(&a,&b));assert(!SudekiMpStoryMovieAdvances(&b,&a));
    a.elapsed_ms=10;b=a;b.elapsed_ms=9;assert(!SudekiMpStoryMovieAdvances(&a,&b));
}
static void inject(SudekiMpLanPartySession *to,SudekiMpLanPartySession *from,
    unsigned kind,SudekiMpStoryMovie event,unsigned mutation) {
    Peer *p=&from->peer[1];uint8_t bytes[MAX_DATAGRAM],body[20];
    assert(SudekiMpStoryMovieEncode(&event,body,20));
    /* send_message emits the exact envelope; receive through the real socket.
     * Wrong-role and stale lease events use otherwise legitimate encodings. */
    uint64_t token=p->status.lease.token;uint32_t generation=p->status.lease.generation;
    if(mutation==1)++token;
    if(mutation==2)++generation;
    (void)bytes;
    assert(send_message(from,&p->address,kind,1,generation,token,next_sequence(p),0,body,20));
    SudekiMpLanPartyPoll(to,GetTickCount());
}
static void transport(void) {
    start_pair();pump(520);SudekiMpStoryMovie started; /* cutscene can already hold world publication */
    assert(!SudekiMpLanPartyMovieBegin(client,GetTickCount(),&started));
    assert(SudekiMpLanPartyMovieBegin(host,GetTickCount(),&started));
    assert(!SudekiMpLanPartyMovieBegin(host,GetTickCount(),&started));
    host->now=host->movie_started_at-1;service_movie_transport(host,&host->peer[1]);
    assert(host->movie.elapsed_ms==0); /* sample-before-lock race */
    SudekiMpStoryMovie v=get_event(1);assert(v.serial==1 && v.epoch==7 && v.revision==9);
    /* No game-thread scene publication during the movie: worker repeats still
     * reach the client and keep the event current after scene freshness dies. */
    pump(580);v=get_event(1);assert(v.elapsed_ms>=1000);
    SudekiMpLanPartyLease lease;assert(SudekiMpLanPartyMovieGet(client,GetTickCount(),&lease,&v));
    assert(!SudekiMpLanPartyGetStoryScene(client,&lease,GetTickCount(),&scene));
    unsigned serial=v.serial;
    for(unsigned mutation=0;mutation<4;++mutation) {
        SudekiMpStoryMovie bad=v;bad.serial++;
        if(mutation==0)bad.revision++;
        if(mutation==3)bad.epoch++;
        inject(client,host,MSG_STORY_MOVIE,bad,mutation);
        assert(client->movie.serial==serial);
    }
    SudekiMpStoryMovie malicious=v;malicious.active=0;malicious.elapsed_ms=SUDEKIMP_STORY_MOVIE_LIMIT_MS;
    inject(host,client,MSG_STORY_MOVIE,malicious,0);assert(host->movie.active);
    /* Terminal survives scene advance and cannot be undone by reordered start. */
    scene=host->story_scene;scene.revision++;scene.epoch++;scene.available_mask=4;scene.leader_seat=2;scene.observed_tick=GetTickCount();
    assert(SudekiMpLanPartyPublishStoryScene(host,&scene));pump(3);
    SudekiMpLanPartyMovieEnd(host,&started,GetTickCount());v=get_event(0);assert(v.serial==1);
    v.active=1;inject(client,host,MSG_STORY_MOVIE,v,0);assert(!client->movie.active);
    /* End-before-start is a terminal tombstone on a current scene. */
    v=(SudekiMpStoryMovie){2,scene.epoch,scene.revision,50,1,0};inject(client,host,MSG_STORY_MOVIE,v,0);
    assert(client->movie.serial==2 && !client->movie.active);
    v.active=1;inject(client,host,MSG_STORY_MOVIE,v,0);assert(!client->movie.active);
    assert(!SudekiMpLanPartyMovieGet(client,GetTickCount()+1001,&lease,&v));
    /* Connection reset clears the journal; old generation cannot restart it. */
    SudekiMpLanPartyLease old=client->peer[1].status.lease;
    assert(SudekiMpLanPartyDisconnect(client,&old));
    assert(!SudekiMpLanPartyMovieGet(client,GetTickCount(),&lease,&v));
    assert(SudekiMpLanPartyClientRejoin(client));assert(!client->movie.serial);
    SudekiMpLanPartyDestroy(client,FALSE);SudekiMpLanPartyDestroy(host,FALSE);
}
static void recipient_and_drop(void) {
    start_pair();SudekiMpStoryMovie started;
    /* A peer without the optional capability is not enrolled. */
    host->peer[1].movie_ready=0;
    assert(SudekiMpLanPartyMovieBegin(host,GetTickCount(),&started));
    assert(!host->movie_recipients[1].token);
    client->peer[1].movie_sent_at=0;
    pump(60);assert(host->peer[1].movie_ready && !client->movie.serial);
    SudekiMpLanPartyMovieEnd(host,&started,GetTickCount());pump(3);
    scene.observed_tick=GetTickCount();assert(SudekiMpLanPartyPublishStoryScene(host,&scene));pump(3);
    assert(SudekiMpLanPartyMovieBegin(host,GetTickCount(),&started));
    SudekiMpLanPartyPoll(host,GetTickCount());
    uint8_t dropped[MAX_DATAGRAM];while(recv(client->socket,(char *)dropped,sizeof(dropped),0)>0){}
    assert(!client->movie.serial);SudekiMpStoryMovie v=get_event(1);assert(v.serial==2);
    /* Neither a late join nor a replaced generation inherits the recipient. */
    ++host->movie_recipients[1].generation;client->movie.serial=0;
    pump(65);assert(!client->movie.serial);
    SudekiMpLanPartyDestroy(client,FALSE);SudekiMpLanPartyDestroy(host,FALSE);
}
static void optional_capability(void) {
    start_pair();
    /* Simulate a host that ignores the added message but understands the
     * unchanged ordinary heartbeat. No movie capability means no enrollment. */
    host->config.story_observation=1;host->peer[1].movie_ready=0;
    host->peer[1].status.transport_confirmed=0;
    pump(600);
    assert(host->peer[1].status.transport_confirmed && !host->peer[1].movie_ready &&
        host->peer[1].status.phase==SUDEKIMP_LAN_PARTY_OBSERVING);
    SudekiMpLanPartyDestroy(client,FALSE);SudekiMpLanPartyDestroy(host,FALSE);
}
int main(void) {codecs();transport();recipient_and_drop();optional_capability();capability_confirms();puts("StoryMovieTransportTest: PASS");return 0;}
