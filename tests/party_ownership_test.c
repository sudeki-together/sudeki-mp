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
    CHECK(s.phase==SUDEKIMP_PARTY_SWAP_IDLE && s.assignment.humans==1);
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
int main(void) {
    every_roster(); lifecycle();
    if(failures) return 1;
    puts("party_ownership_test: PASS (policy only, no native handoff)"); return 0;
}
