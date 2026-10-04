#include "engine/party_ownership.h"
#include <stdio.h>
#include <string.h>

static unsigned failures;
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"FAIL %u: %s\n",__LINE__,#x); ++failures; } } while(0)
static SudekiMpPartyOwnership initial(unsigned humans) {
    SudekiMpPartyAssignment a={0}; SudekiMpPartyOwnership s={0};
    a.world=1; a.revision=1; a.humans=(uint8_t)humans; a.available=15;
    for(unsigned p=0;p<4;++p) { a.generation[p]=1; a.character[p]=(humans&(1u<<p))?(uint8_t)p:4; }
    CHECK(SudekiMpPartyOwnershipInitialize(&s,&a)); return s;
}
static void every_roster(void) {
    for(unsigned mask=1;mask<16;mask+=2) for(unsigned p=0;p<4;++p) for(unsigned c=0;c<4;++c) {
        SudekiMpPartyOwnership s=initial(mask),before=s;
        SudekiMpPartySwapResult r=SudekiMpPartySwapRequest(&s,p,c,1,1,1,0,0);
        if(!(mask&(1u<<p))) CHECK(r==SUDEKIMP_PARTY_SWAP_INVALID);
        else if(p==c) CHECK(r==SUDEKIMP_PARTY_SWAP_SAME_CHARACTER);
        else if(mask&(1u<<c)) CHECK(r==SUDEKIMP_PARTY_SWAP_OCCUPIED);
        else {
            CHECK(r==SUDEKIMP_PARTY_SWAP_OK);
            CHECK(!SudekiMpPartyPlayerInputReady(&s,p,p,1,1,1));
            CHECK(!SudekiMpPartySwapCommit(&s,p,1));
            CHECK(SudekiMpPartySwapBegin(&s,p,1));
            CHECK(!SudekiMpPartySwapCancel(&s,p,1));
            CHECK(!SudekiMpPartyOwnershipLeaveDrained(&s,p));
            CHECK(SudekiMpPartySwapCommit(&s,p,1));
            CHECK(s.assignment.character[p]==c && s.assignment.revision==2);
            CHECK(s.assignment.generation[p]==2 && s.assignment.generation[c]==2);
            CHECK(!SudekiMpPartyPlayerInputReady(&s,p,c,1,2,2));
            CHECK(!SudekiMpPartySwapAcknowledge(&s,p,1,1,1));
            CHECK(SudekiMpPartySwapAcknowledge(&s,p,1,1,2));
            CHECK(SudekiMpPartyPlayerInputReady(&s,p,c,1,2,2));
            CHECK(!SudekiMpPartyPlayerInputReady(&s,p,p,1,1,1));
            CHECK(SudekiMpPartyCharacterOwner(&s.assignment,p)==4);
            CHECK(SudekiMpPartyCharacterOwner(&s.assignment,c)==p);
            CHECK(SudekiMpPartyAssignmentValid(&s.assignment));
            CHECK(SudekiMpPartySwapRequest(&s,p,p,1,1,2,0,0)==SUDEKIMP_PARTY_SWAP_STALE);
        }
        if(r!=SUDEKIMP_PARTY_SWAP_OK) CHECK(!memcmp(&s,&before,sizeof(s)));
    }
}
static void lifecycle(void) {
    SudekiMpPartyOwnership s=initial(3),saved;
    CHECK(SudekiMpPartySwapRequest(&s,1,2,1,2,1,0,0)==SUDEKIMP_PARTY_SWAP_STALE);
    CHECK(SudekiMpPartySwapRequest(&s,1,2,1,1,2,0,0)==SUDEKIMP_PARTY_SWAP_STALE);
    CHECK(SudekiMpPartySwapRequest(&s,1,2,1,1,1,4,0)==SUDEKIMP_PARTY_SWAP_BUSY);
    CHECK(SudekiMpPartySwapRequest(&s,1,2,1,1,1,0,1)==SUDEKIMP_PARTY_SWAP_BUSY);
    s.assignment.available=11;
    CHECK(SudekiMpPartySwapRequest(&s,1,2,1,1,1,0,0)==SUDEKIMP_PARTY_SWAP_UNAVAILABLE);
    s.assignment.available=15;
    CHECK(SudekiMpPartySwapRequest(&s,1,2,1,1,1,0,0)==SUDEKIMP_PARTY_SWAP_OK);
    CHECK(SudekiMpPartySwapRequest(&s,0,2,1,1,1,0,0)==SUDEKIMP_PARTY_SWAP_BUSY);
    CHECK(SudekiMpPartyPlayerInputReady(&s,0,0,1,1,1));
    CHECK(!SudekiMpPartySwapCancel(&s,0,1));
    CHECK(SudekiMpPartySwapCancel(&s,1,1));
    CHECK(SudekiMpPartySwapRequest(&s,1,2,2,1,1,0,0)==SUDEKIMP_PARTY_SWAP_OK);
    CHECK(SudekiMpPartySwapBegin(&s,1,2));
    CHECK(SudekiMpPartySwapAbortRestored(&s,1,2));
    CHECK(s.assignment.character[1]==1 && s.assignment.revision==2);
    CHECK(!SudekiMpPartyPlayerInputReady(&s,1,1,1,1,1));
    CHECK(SudekiMpPartyPlayerInputReady(&s,1,1,1,2,2));
    CHECK(SudekiMpPartySwapRequest(&s,1,2,3,1,2,0,0)==SUDEKIMP_PARTY_SWAP_OK);
    CHECK(SudekiMpPartySwapBegin(&s,1,3)); CHECK(SudekiMpPartySwapCommit(&s,1,3));
    CHECK(SudekiMpPartyOwnershipLeaveDrained(&s,1)); /* disconnect before ACK */
    CHECK(s.phase==SUDEKIMP_PARTY_SWAP_IDLE && s.assignment.humans==3);
    CHECK(s.connected==1 && s.assignment.character[1]==2);
    CHECK(!SudekiMpPartyOwnershipJoin(&s,1,3));
    CHECK(SudekiMpPartyReleaseReservation(&s,0,1,1,1,s.assignment.revision)==SUDEKIMP_PARTY_SWAP_OK);
    CHECK(SudekiMpPartyOwnershipJoin(&s,1,3));
    CHECK(s.last_request[1]==0);
    CHECK(!SudekiMpPartyOwnershipJoin(&s,2,3));
    CHECK(!SudekiMpPartyOwnershipLeaveDrained(&s,0));
    saved=s; s.assignment.revision=UINT32_MAX;
    CHECK(SudekiMpPartySwapRequest(&s,1,1,4,1,UINT32_MAX,0,0)==SUDEKIMP_PARTY_SWAP_EXHAUSTED);
    s=saved; s.assignment.generation[1]=UINT32_MAX;
    CHECK(SudekiMpPartySwapRequest(&s,1,1,4,1,s.assignment.revision,0,0)==SUDEKIMP_PARTY_SWAP_EXHAUSTED);
    s=saved; s.assignment.character[1]=s.assignment.character[0];
    CHECK(!SudekiMpPartyAssignmentValid(&s.assignment));
    CHECK(!SudekiMpPartyPlayerInputReady(&s,0,0,1,s.assignment.revision,1));
}

static uint32_t generation(const SudekiMpPartyOwnership *s,unsigned p) {
    return s->assignment.generation[s->assignment.character[p]];
}
static int ready(const SudekiMpPartyOwnership *s,unsigned p) {
    unsigned c=s->assignment.character[p];
    return c<4 && SudekiMpPartyPlayerInputReady(s,p,c,s->assignment.world,
        s->assignment.revision,s->assignment.generation[c]);
}
static SudekiMpPartySwapResult presence(SudekiMpPartyOwnership *s,unsigned p,int menu,int away) {
    return SudekiMpPartyPresenceRequest(s,p,s->last_request[p]+1,s->assignment.world,
        s->assignment.revision,menu,away);
}
static void acquire(SudekiMpPartyOwnership *s,unsigned p) {
    uint32_t g=generation(s,p), revision=s->assignment.revision;
    CHECK(SudekiMpPartyControlAcquireBegin(s,p,1,revision,g));
    CHECK(!ready(s,p));
    CHECK(!SudekiMpPartyControlAcknowledge(s,p,1,revision,g));
    CHECK(SudekiMpPartyControlAcquireCommit(s,p,1,g));
    CHECK(!ready(s,p));
    CHECK(!SudekiMpPartyControlAcknowledge(s,p,1,revision,g));
    if(s->phase==SUDEKIMP_PARTY_SWAP_WAIT_ACK && s->player==p) {
        CHECK(!SudekiMpPartyControlAcknowledge(s,p,1,s->assignment.revision,generation(s,p)));
        CHECK(SudekiMpPartySwapAcknowledge(s,p,s->request,1,s->assignment.revision));
    } else CHECK(SudekiMpPartyControlAcknowledge(s,p,1,s->assignment.revision,generation(s,p)));
    CHECK(SudekiMpPartyOwnershipValid(s));
}
static void menu_and_away(void) {
    SudekiMpPartyOwnership s=initial(15), before;
    CHECK(s.absence_policy==SUDEKIMP_PARTY_AI_COVER);
    for(unsigned p=0;p<4;++p) {
        uint32_t g=generation(&s,p);
        CHECK(presence(&s,p,1,0)==SUDEKIMP_PARTY_SWAP_OK);
        CHECK(!s.paused && !ready(&s,p));
        CHECK(s.control[p]==SUDEKIMP_PARTY_CONTROL_DRAINING);
        CHECK(SudekiMpPartyCharacterOwner(&s.assignment,p)==p);
        CHECK(!SudekiMpPartyControlDrainComplete(&s,p,2,g));
        CHECK(!SudekiMpPartyControlDrainComplete(&s,p,1,g+1));
        CHECK(SudekiMpPartyControlDrainComplete(&s,p,1,g));
        CHECK(!(s.controlling&(1u<<p)));
        CHECK(!SudekiMpPartyControlAcquireBegin(&s,p,1,s.assignment.revision,generation(&s,p)));
        CHECK(presence(&s,p,0,0)==SUDEKIMP_PARTY_SWAP_OK);
        acquire(&s,p); CHECK(ready(&s,p));
    }
    CHECK(presence(&s,1,1,1)==SUDEKIMP_PARTY_SWAP_OK);
    CHECK(!s.paused);
    CHECK(SudekiMpPartyControlDrainComplete(&s,1,1,generation(&s,1)));
    CHECK(presence(&s,1,0,1)==SUDEKIMP_PARTY_SWAP_OK);
    CHECK(!ready(&s,1) && (s.away&2) && !(s.menu&2));
    CHECK(!SudekiMpPartyControlAcquireBegin(&s,1,1,s.assignment.revision,generation(&s,1)));
    CHECK(SudekiMpPartySetAbsencePolicy(&s,0,s.last_request[0]+1,1,s.assignment.revision,
        SUDEKIMP_PARTY_SHARED_PAUSE)==SUDEKIMP_PARTY_SWAP_OK);
    CHECK(!s.paused); /* Policy changes are not retroactive. */
    CHECK(presence(&s,1,0,1)==SUDEKIMP_PARTY_SWAP_OK && !s.paused);
    CHECK(presence(&s,1,0,0)==SUDEKIMP_PARTY_SWAP_OK);
    acquire(&s,1);
    CHECK(presence(&s,1,1,0)==SUDEKIMP_PARTY_SWAP_OK && !s.paused);
    CHECK(presence(&s,1,1,1)==SUDEKIMP_PARTY_SWAP_OK && s.paused);
    CHECK(!ready(&s,0) && !ready(&s,2));
    before=s;
    CHECK(SudekiMpPartySetPaused(&s,1,s.last_request[1]+1,1,s.assignment.revision,0)
        ==SUDEKIMP_PARTY_SWAP_UNAUTHORIZED);
    CHECK(!memcmp(&s,&before,sizeof(s)));
    CHECK(SudekiMpPartySetPaused(&s,0,s.last_request[0]+1,1,s.assignment.revision,0)
        ==SUDEKIMP_PARTY_SWAP_OK);
    CHECK(ready(&s,0));
    CHECK(presence(&s,1,0,1)==SUDEKIMP_PARTY_SWAP_OK && !s.paused);
    CHECK(SudekiMpPartyControlDrainComplete(&s,1,1,generation(&s,1)));
    CHECK(presence(&s,1,0,0)==SUDEKIMP_PARTY_SWAP_OK);
    CHECK(presence(&s,1,0,1)==SUDEKIMP_PARTY_SWAP_OK && s.paused);
    CHECK(SudekiMpPartyOwnershipValid(&s));
}
static void disconnect_and_reservation(void) {
    SudekiMpPartyOwnership s=initial(3);
    uint32_t g=generation(&s,1), request;
    CHECK(SudekiMpPartyOwnershipDisconnect(&s,1));
    CHECK(!ready(&s,1) && s.assignment.humans==3 && s.connected==1);
    CHECK(s.assignment.character[1]==1 && s.control[1]==SUDEKIMP_PARTY_CONTROL_DRAINING);
    CHECK(!SudekiMpPartyOwnershipConnect(&s,1));
    CHECK(SudekiMpPartySwapRequest(&s,0,1,1,1,s.assignment.revision,0,0)
        ==SUDEKIMP_PARTY_SWAP_OCCUPIED);
    CHECK(SudekiMpPartyControlDrainComplete(&s,1,1,g));
    CHECK(SudekiMpPartyOwnershipConnect(&s,1));
    CHECK(s.last_request[1]==0 && s.assignment.character[1]==1 && !ready(&s,1));
    acquire(&s,1); CHECK(ready(&s,1));
    CHECK(presence(&s,1,0,1)==SUDEKIMP_PARTY_SWAP_OK);
    CHECK(SudekiMpPartyReleaseReservation(&s,0,1,1,1,s.assignment.revision)
        ==SUDEKIMP_PARTY_SWAP_BUSY);
    CHECK(SudekiMpPartyControlDrainComplete(&s,1,1,generation(&s,1)));
    request=s.last_request[0]+1;
    CHECK(SudekiMpPartyHostReassignRequest(&s,0,0,1,request,1,s.assignment.revision,0,0)
        ==SUDEKIMP_PARTY_SWAP_OK);
    CHECK(!SudekiMpPartyControlAcquireBegin(&s,1,1,s.assignment.revision,generation(&s,1)));
    CHECK(SudekiMpPartySwapBegin(&s,0,request));
    CHECK(SudekiMpPartySwapCommit(&s,0,request));
    CHECK(SudekiMpPartySwapAcknowledge(&s,0,request,1,s.assignment.revision));
    CHECK(s.assignment.character[0]==1 && s.assignment.character[1]==4);
    CHECK(ready(&s,0));
    CHECK(presence(&s,1,0,0)==SUDEKIMP_PARTY_SWAP_OK);
    CHECK(!ready(&s,1)); /* Return never displaces the new owner. */
    request=s.last_request[1]+1;
    CHECK(SudekiMpPartySwapRequest(&s,1,0,request,1,s.assignment.revision,0,0)
        ==SUDEKIMP_PARTY_SWAP_OK);
    CHECK(SudekiMpPartySwapBegin(&s,1,request));
    CHECK(SudekiMpPartySwapCommitReleased(&s,1,request));
    CHECK(!SudekiMpPartySwapAcknowledge(&s,1,request,1,s.assignment.revision));
    CHECK(!ready(&s,1)); acquire(&s,1); CHECK(ready(&s,1));
    CHECK(presence(&s,0,0,1)==SUDEKIMP_PARTY_SWAP_OK);
    CHECK(SudekiMpPartyControlDrainComplete(&s,0,1,generation(&s,0)));
    CHECK(SudekiMpPartyReleaseReservation(&s,0,0,s.last_request[0]+1,1,s.assignment.revision)
        ==SUDEKIMP_PARTY_SWAP_OK);
    CHECK((s.connected&1) && s.assignment.character[0]==4); /* Host spectator. */
    CHECK(!SudekiMpPartyOwnershipDisconnect(&s,0));
    CHECK(SudekiMpPartyOwnershipValid(&s));
}
static void transaction_edges(void) {
    SudekiMpPartyOwnership s=initial(3), before;
    uint32_t g, request;
    CHECK(presence(&s,1,1,0)==SUDEKIMP_PARTY_SWAP_OK);
    /* Closing the menu before native drain does not pretend drain completed. */
    CHECK(presence(&s,1,0,0)==SUDEKIMP_PARTY_SWAP_OK);
    CHECK(!ready(&s,1));
    CHECK(SudekiMpPartyControlDrainComplete(&s,1,1,generation(&s,1)));
    g=generation(&s,1);
    CHECK(SudekiMpPartyControlAcquireBegin(&s,1,1,s.assignment.revision,g));
    CHECK(SudekiMpPartyOwnershipDisconnect(&s,1));
    CHECK(!SudekiMpPartyOwnershipLeaveDrained(&s,1));
    CHECK(SudekiMpPartyControlAcquireCommit(&s,1,1,g));
    CHECK(s.control[1]==SUDEKIMP_PARTY_CONTROL_DRAINING);
    CHECK(SudekiMpPartyControlDrainComplete(&s,1,1,generation(&s,1)));
    CHECK(SudekiMpPartyOwnershipConnect(&s,1));
    g=generation(&s,1);
    CHECK(SudekiMpPartyControlAcquireBegin(&s,1,1,s.assignment.revision,g));
    CHECK(SudekiMpPartyControlAcquireAbortRestored(&s,1,1,g));
    CHECK(s.control[1]==SUDEKIMP_PARTY_CONTROL_AI && !ready(&s,1));
    acquire(&s,1);
    request=s.last_request[1]+1;
    CHECK(SudekiMpPartySwapRequest(&s,1,2,request,1,s.assignment.revision,0,0)
        ==SUDEKIMP_PARTY_SWAP_OK);
    CHECK(SudekiMpPartySwapBegin(&s,1,request));
    CHECK(SudekiMpPartyOwnershipDisconnect(&s,1));
    CHECK(!SudekiMpPartyOwnershipLeaveDrained(&s,1));
    CHECK(SudekiMpPartySwapCommit(&s,1,request));
    CHECK(!SudekiMpPartySwapAcknowledge(&s,1,request,1,s.assignment.revision));
    CHECK(SudekiMpPartyOwnershipLeaveDrained(&s,1));
    CHECK(s.assignment.character[1]==2 && s.connected==1 && s.controlling==1);
    before=s;
    CHECK(SudekiMpPartyReleaseReservation(&s,0,1,s.last_request[0]+1,2,s.assignment.revision)
        ==SUDEKIMP_PARTY_SWAP_STALE);
    CHECK(!memcmp(&s,&before,sizeof(s)));
    s.assignment.generation[2]=UINT32_MAX; before=s;
    CHECK(SudekiMpPartyReleaseReservation(&s,0,1,s.last_request[0]+1,1,s.assignment.revision)
        ==SUDEKIMP_PARTY_SWAP_EXHAUSTED);
    CHECK(!memcmp(&s,&before,sizeof(s)));
}
static void empty_host(void) {
    SudekiMpPartyOwnership s={0}; SudekiMpPartyAssignment a={0};
    a.world=a.revision=1; a.available=15;
    for(unsigned p=0;p<4;++p) { a.character[p]=4; a.generation[p]=1; }
    CHECK(SudekiMpPartyAssignmentValid(&a));
    CHECK(!SudekiMpPartyOwnershipInitialize(&s,&a));
    CHECK(SudekiMpPartyOwnershipInitializePresence(&s,&a,1,0));
    CHECK(SudekiMpPartyOwnershipConnect(&s,1));
    CHECK(s.connected==3 && !s.assignment.humans && !s.controlling);
    CHECK(!SudekiMpPartyOwnershipInitializePresence(&s,&a,3,2));
    CHECK(SudekiMpPartySwapRequest(&s,0,3,1,1,s.assignment.revision,0,0)==SUDEKIMP_PARTY_SWAP_OK);
    CHECK(SudekiMpPartySwapBegin(&s,0,1)); CHECK(SudekiMpPartySwapCommitReleased(&s,0,1));
    CHECK(!SudekiMpPartySwapAcknowledge(&s,0,1,1,s.assignment.revision));
    CHECK(s.assignment.character[0]==3 && !ready(&s,0));
    acquire(&s,0); CHECK(ready(&s,0));
}
int main(void) {
    every_roster(); lifecycle(); menu_and_away(); disconnect_and_reservation();
    transaction_edges(); empty_host();
    if(failures) return 1;
    puts("party_ownership_test: PASS (policy only, no native handoff)"); return 0;
}
