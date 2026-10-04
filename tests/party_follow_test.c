#include "engine/party_follow.h"
#include <math.h>
#include <stdio.h>

#define CHECK(x) do {if(!(x)){fprintf(stderr,"follow: line %d\n",__LINE__);return 1;}}while(0)
int main(void) {
    float p[4][3]={{0,0,0},{10,0,0},{8,0,0},{1,0,0}};
    const float (*positions)[3]=(const float (*)[3])p;
    CHECK(SudekiMpPartyFollowTarget(2,0,15,3,positions)==0); /* active host wins */
    CHECK(SudekiMpPartyFollowTarget(2,4,15,3,positions)==1);
    CHECK(SudekiMpPartyFollowTarget(3,4,15,3,positions)==0); /* per-AI nearest */
    CHECK(SudekiMpPartyFollowTarget(2,0,15,2,positions)==1); /* host Away */
    CHECK(SudekiMpPartyFollowTarget(0,4,15,2,positions)==1); /* host AI follows */
    CHECK(SudekiMpPartyFollowTarget(2,0,15,0,positions)==4);
    CHECK(SudekiMpPartyFollowTarget(0,0,15,1,positions)==4); /* never human */
    p[2][0]=5; CHECK(SudekiMpPartyFollowTarget(2,4,15,3,positions)==0);
    CHECK(SudekiMpPartyFollowTarget(2,4,7,3,positions)==0);
    CHECK(SudekiMpPartyFollowTarget(2,4,7,8,positions)==4);
    CHECK(SudekiMpPartyFollowTarget(2,4,3,3,positions)==4);
    CHECK(SudekiMpPartyFollowTarget(4,4,15,3,positions)==4);
    CHECK(SudekiMpPartyFollowTarget(2,5,15,3,positions)==4);
    CHECK(SudekiMpPartyFollowTarget(2,4,255,3,positions)==4);
    p[0][0]=NAN; CHECK(SudekiMpPartyFollowTarget(2,4,15,3,positions)==4);
    p[0][0]=INFINITY; CHECK(SudekiMpPartyFollowTarget(2,4,15,3,positions)==4);
    p[0][0]=1000000; CHECK(SudekiMpPartyFollowTarget(2,4,15,3,positions)==4);
    CHECK(SudekiMpPartyFollowTarget(2,4,15,3,NULL)==4);
    puts("party_follow_test: PASS"); return 0;
}
